// main.cpp
// Build: see instructions below

#define SDL_MAIN_HANDLED
#pragma comment(linker, "/SUBSYSTEM:CONSOLE")
#include <glad/glad.h>
#include <SDL2/SDL.h>
#include <iostream>
#include <string>
#include <chrono>
#include <algorithm> // for std::max

#pragma comment(lib, "opengl32.lib")

// Minimal shader loader for GL2/GL3 compatibility using shader strings.
// The source code for the vertex shader stored in a const char array
static const char* vertexShaderSrc = R"glsl(
#version 330 core
layout(location = 0) in vec2 inPos;
out vec2 uv;
void main(){
    uv = inPos * 0.5 + 0.5;
    gl_Position = vec4(inPos, 0.0, 1.0);
}
)glsl";

static const char* fragmentShaderSrc = R"glsl(
#version 330 core

out vec4 FragColor;

uniform vec2 uResolution;
uniform float uTime;

/*
    Corner-bending tunnel
    - The viewer travels through a tunnel whose center path makes eased 90° turns.
    - Path is piecewise cardinal directions (right, up, left, down) with smooth easing between them.
    - We simulate "bows" by easing the direction change and adding banking + angular warp.
    - Screen-space tunnel mapping for performance: no raymarch, just polar distortion.

    Controls:
    - uTime: travel speed and turn cadence.
    - uResolution: viewport size.
*/

float smoothstep01(float x) {
    return smoothstep(0.0, 1.0, clamp(x, 0.0, 1.0));
}

// Cubic ease in/out (s-curve) for pleasant corner bows
float easeInOut(float t) {
    t = clamp(t, 0.0, 1.0);
    return t*t*(3.0 - 2.0*t);
}

// Rotate 2D vector by angle
mat2 rot(float a) {
    float s = sin(a), c = cos(a);
    return mat2(c, -s, s, c);
}

// Piecewise cardinal directions with smoothed 90° turns.
// Each segment lasts segLen in "z travel"; we blend directions across a blend window.
vec2 pathDirection(float z) {
    float segLen = 6.0;                // length of straight segment (in tunnel units)
    float blendLen = 2.2;              // how long the corner easing lasts
    float t = z / segLen;              // segment index space
    float i = floor(t);                // which segment we’re in
    float f = fract(t);                // local progress in this segment (0..1)

    // Determine base direction (cardinals cycle: +X, +Y, -X, -Y)
    int idx = int(mod(i, 4.0));
    vec2 dir;
    if (idx == 0) dir = vec2(1.0, 0.0);
    else if (idx == 1) dir = vec2(0.0, 1.0);
    else if (idx == 2) dir = vec2(-1.0, 0.0);
    else              dir = vec2(0.0, -1.0);

    // Next direction for blending during corner
    int idxNext = int(mod(i + 1.0, 4.0));
    vec2 dirNext;
    if (idxNext == 0) dirNext = vec2(1.0, 0.0);
    else if (idxNext == 1) dirNext = vec2(0.0, 1.0);
    else if (idxNext == 2) dirNext = vec2(-1.0, 0.0);
    else                   dirNext = vec2(0.0, -1.0);

    // Determine ease factor: only active near the end of the segment
    float cornerStart = 1.0 - (blendLen / segLen);
    float w = easeInOut((f - cornerStart) / (1.0 - cornerStart));

    // Blend directions to create a smooth "bow" around the corner
    vec2 blended = normalize(mix(dir, dirNext, w));
    return blended;
}

// Compute the center offset of the tunnel along the path
vec2 pathCenter(float z) {
    float segLen = 6.0;
    float i = floor(z / segLen);
    float f = fract(z / segLen);

    // Accumulate position across segments
    // For each full segment, move in its cardinal direction by segLen*(1 - blendFrac)
    // Then add the current segment’s eased displacement.
    // For a simple and stable result, we integrate locally with direction and a mild lateral bow.
    vec2 dir = pathDirection(z);
    // Per-corner bow amplitude (lateral offset that creates a rounded feel)
    float bowAmp = 1.6;
    float turnPhase = easeInOut(f); // 0..1 across the segment
    // Lateral bow perpendicular to current direction
    vec2 perp = vec2(-dir.y, dir.x);
    float bow = bowAmp * sin(3.14159 * turnPhase) * smoothstep01(turnPhase);

    // Forward accumulation approximation: move along dir scaled by local progress
    // This keeps the center advancing steadily; we don’t need exact global integration for the effect.
    vec2 forward = dir * (f * segLen);

    return forward + perp * bow;
}

// Procedural texture for the tunnel walls
vec3 tunnelMaterial(vec2 uv, float r, float a, float z) {
    // Rings and stripes with slight time-driven motion
    float rings = sin(10.0 * r - 0.6 * z);
    float stripes = sin(8.0 * a + 1.2 * z);
    float mixv = 0.5 + 0.5 * rings * stripes;

    // Palette: warm to cool across angle
    vec3 baseA = vec3(0.10, 0.25, 0.90);
    vec3 baseB = vec3(0.95, 0.30, 0.10);
    vec3 col = mix(baseA, baseB, 0.5 + 0.5 * sin(a * 2.0));
    col *= 0.6 + 0.4 * mixv;

    // Radial falloff for vignette inside the tunnel
    float v = smoothstep(1.4, 0.2, r);
    col *= 0.6 + 0.4 * v;

    return col;
}

void main() {
    // Normalize coordinates
    vec2 p = (gl_FragCoord.xy * 2.0 - uResolution.xy) / uResolution.y;

    float time = uTime;

    // Travel speed and depth
    float speed = 1.6;
    float z = time * speed;

    // Path orientation and banking (roll around the tunnel axis)
    vec2 dir = pathDirection(z);
    float bank = 0.6 * sin(0.7 * z); // gentle banking that responds to turns
    mat2 bankRot = rot(bank);

    // Offset the center based on path (bows around corners)
    vec2 center = pathCenter(z);
    // Smooth global offset accumulation to avoid drift explosion:
    // Keep center moderately bounded by damping.
    center *= 0.15;

    // Transform screen space by banking and center offset
    vec2 q = bankRot * (p - center);

    // Polar mapping: classic tunnel
    float r = length(q);
    float a = atan(q.y, q.x);

    // Axial repetition to give the sense of forward motion
    float repeat = 6.0;
    float axial = (z + 1.5 * r);
    float stripePhase = mod(axial, repeat) / repeat;

    // Slight angular warp tied to corner easing for "bow" feel inside the tube
    float turnEase = easeInOut(fract((z / 6.0)));
    a += 0.35 * turnEase * sin(2.0 * a);

    // Base color
    vec3 base = tunnelMaterial(q, r, a, z);

    // Chromatic separation (subtle)
    float ca = 0.004 + 0.002 * turnEase;
    vec2 qR = bankRot * (p - center) + vec2(ca, 0.0);
    vec2 qG = bankRot * (p - center);
    vec2 qB = bankRot * (p - center) - vec2(ca, 0.0);

    float rR = length(qR);
    float rG = length(qG);
    float rB = length(qB);
    float aR = atan(qR.y, qR.x);
    float aG = atan(qG.y, qG.x);
    float aB = atan(qB.y, qB.x);

    vec3 col;
    col.r = tunnelMaterial(qR, rR, aR, z).r;
    col.g = tunnelMaterial(qG, rG, aG, z).g;
    col.b = tunnelMaterial(qB, rB, aB, z).b;

    // Inner glow near the axis for speed lines
    float glow = exp(-10.0 * r);
    col += vec3(0.9, 0.9, 1.0) * glow * (0.5 + 0.5 * sin(1.5 * z));

    // Segment markers for depth cues
    float rings = 0.5 + 0.5 * sin(10.0 * r - 0.6 * z);
    col *= 0.8 + 0.2 * rings;

    // Final tone mapping
    col = pow(col, vec3(0.9)); // mild gamma tweak

    FragColor = vec4(p*0.5+0.5, 0.0, 1.0);
}
)glsl";



static GLuint compileShader(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok = 0; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[4096]; glGetShaderInfoLog(sh, sizeof(buf), nullptr, buf);
        std::cerr << "Shader compile error: " << buf << "\n";
        return 0;
    }
    return sh;
}

static GLuint linkProgram(GLuint v, GLuint f) {
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glBindAttribLocation(p, 0, "inPos");
    glLinkProgram(p);
    GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[4096]; glGetProgramInfoLog(p, sizeof(buf), nullptr, buf);
        std::cerr << "Program link error: " << buf << "\n";
        return 0;
    }
    return p;
}

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }
    //SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    int w = 1280, h = 720;
    SDL_Window* win = SDL_CreateWindow("Plasma Time Warp Tunnel", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!win) { std::cerr << "CreateWindow failed\n"; return 1; }

    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) { std::cerr << "CreateContext failed\n"; return 1; }

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return 1;
    }
    std::cout << "OpenGL: " << (const char*)glGetString(GL_VERSION) << "\n";

    // Load OpenGL functions if needed; on many setups gl* from <GL/gl.h> is enough.
    // Create shaders
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);
    if (!vs || !fs) return 1;
    GLuint prog = linkProgram(vs, fs);
    glDeleteShader(vs); glDeleteShader(fs);
    if (!prog) return 1;

    // Fullscreen triangle VAO/VBO
    GLuint vao, vbo;
    float verts[] = { -1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f }; // odd trick: single triangle covering screen
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Uniform locations
    GLint locTime = glGetUniformLocation(prog, "iTime");
    GLint locRes = glGetUniformLocation(prog, "iResolution");
    GLint locSpeed = glGetUniformLocation(prog, "speed");
    GLint locWarp = glGetUniformLocation(prog, "warp");
    GLint locThickness = glGetUniformLocation(prog, "thickness");
    GLint locColorShift = glGetUniformLocation(prog, "colorShift");

    auto start = std::chrono::high_resolution_clock::now();
    bool running = true;
    SDL_Event e;
    // default params
    float speed = 6.0f;
    float warp = 1.0f;
    float thickness = 0.18f;
    float colorShift = 0.0f;

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) running = false;
                if (e.key.keysym.sym == SDLK_UP) speed *= 1.1f;
                if (e.key.keysym.sym == SDLK_DOWN) speed /= 1.1f;
                if (e.key.keysym.sym == SDLK_LEFT) warp = std::max(0.1f, warp - 0.1f);
                if (e.key.keysym.sym == SDLK_RIGHT) warp += 0.1f;
                if (e.key.keysym.sym == SDLK_z) thickness = std::max(0.01f, thickness - 0.01f);
                if (e.key.keysym.sym == SDLK_x) thickness += 0.01f;
                if (e.key.keysym.sym == SDLK_c) colorShift += 0.05f;
                if (e.key.keysym.sym == SDLK_v) colorShift -= 0.05f;
            }
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                w = e.window.data1; h = e.window.data2;
                glViewport(0, 0, w, h);
            }
        }

        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> diff = now - start;
        float t = diff.count();

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(prog);
        glUniform1f(locTime, t);
        glUniform2f(locRes, (float)w, (float)h);
        glUniform1f(locSpeed, speed);
        glUniform1f(locWarp, warp);
        glUniform1f(locThickness, thickness);
        glUniform1f(locColorShift, colorShift);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        SDL_GL_SwapWindow(win);
        SDL_Delay(1);
    }

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}