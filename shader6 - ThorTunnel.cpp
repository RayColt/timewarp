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
uniform float uSpeed;
uniform float warp;
uniform float thickness;
uniform float colorShift;

/*
 Dramatic "Thor hammer" travel through bending warp tunnels.
 - Strong angular warp, banking, and vortex streaks create violent twist.
 - Procedural hammer silhouette rides the axis to sell the swinging hammer feeling.
 - Controls:
   * warp: overall angular warping intensity (0..3)
   * thickness: wall thickness / ring softness (0.2..2.0)
   * colorShift: hue rotation in radians
*/

float PI = 3.14159265358979323846;

mat2 rot(float a) {
    float s = sin(a), c = cos(a);
    return mat2(c, -s, s, c);
}

float easeInOut(float t) {
    t = clamp(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// simple pseudo-hue rotate (approx)
vec3 hueShift(vec3 c, float angle) {
    float ca = cos(angle), sa = sin(angle);
    mat3 m = mat3(
        vec3(0.299, 0.587, 0.114),
        vec3(0.299, 0.587, 0.114),
        vec3(0.299, 0.587, 0.114)
    );
    mat3 p = mat3(
        vec3(0.701, -0.587, -0.114),
        vec3(-0.299, 0.413, -0.114),
        vec3(-0.3, -0.588, 0.886)
    );
    return clamp((m + ca * p + sa * (p * mat3(
        vec3(0.0), vec3(0.0), vec3(0.0)
    ))) * c, 0.0, 1.0);
}

// stylized hammer silhouette at axis: long handle + rectangular head
float hammerMask(vec2 uv, float t) {
    // uv in tunnel local coordinates: y along handle, x radial
    // scale and bob the hammer along t (axial travel)
    float travel = mod(t * 1.6, 8.0); // loop travel speed for effect
    float zpos = -fract(travel) * 2.0 + 0.4; // moves toward camera
    // simulated depth scale
    float scale = mix(0.9, 0.25, clamp((zpos + 1.0), 0.0, 1.0));

    vec2 p = uv / scale;
    // handle: narrow rectangle along y
    float handle = smoothstep(0.02, 0.01, abs(p.x)) * smoothstep(0.6, 0.3, abs(p.y - (0.3 - 0.8*zpos)));
    // head: wide rectangle near top of handle
    vec2 headPos = p - vec2(0.0, -0.15 - 0.5*zpos);
    float headRect = smoothstep(0.35 + 0.02*scale, 0.33 + 0.02*scale, max(abs(headPos.x), abs(headPos.y*0.4)));
    // combine: head over handle (head stronger)
    float mask = clamp(headRect + handle*0.7, 0.0, 1.0);
    // soft edge
    return smoothstep(0.15, 0.0, 1.0 - mask);
}

vec3 palette(float a, float r, float t) {
    // dramatic palette: electric cool -> molten warm depending on angle and time
    vec3 c1 = vec3(0.12, 0.25, 1.0);
    vec3 c2 = vec3(1.0, 0.45, 0.08);
    float wheel = 0.5 + 0.5 * sin(a * 2.0 + t * 0.8 + colorShift);
    vec3 col = mix(c1, c2, wheel);
    // radial darkening
    col *= 0.5 + 0.5 * smoothstep(1.6, 0.2, r);
    return col;
}

float ringsPattern(float r, float z, float thickness) {
    // thicker -> softer rings; thinner -> tight crisp rings
    float freq = mix(18.0, 6.0, smoothstep(0.2, 2.0, thickness));
    float rim = sin(freq * r - 0.9 * z);
    // sharpen by thickness
    float sharp = smoothstep(0.2, 0.5, rim * 0.8 + 0.2 * thickness);
    return sharp;
}

void main() {
    // normalized pixel coords
    vec2 uv = (gl_FragCoord.xy * 2.0 - uResolution.xy) / uResolution.y;

    // travel depth
    float z = uTime * uSpeed;

    // banking: amplify for violent swing
    float bank = 0.9 * sin(0.9 * z);
    mat2 bankRot = rot(bank);

    // path center: stronger lateral bows for exaggerated corners
    float segLen = 6.0;
    float segIdx = floor(z / segLen);
    float segFrac = fract(z / segLen);
    // choose cardinal directions
    int idx = int(mod(segIdx, 4.0));
    vec2 dir;
    if (idx == 0) dir = vec2(1.0, 0.0);
    else if (idx == 1) dir = vec2(0.0, 1.0);
    else if (idx == 2) dir = vec2(-1.0, 0.0);
    else dir = vec2(0.0, -1.0);
    float bowAmp = 2.4; // stronger bow for dramatic bending
    float bowPhase = easeInOut(segFrac);
    vec2 perp = vec2(-dir.y, dir.x);
    vec2 center = dir * (segFrac * segLen * 0.75) + perp * bowAmp * sin(PI * bowPhase) * smoothstep(0.0, 1.0, bowPhase);
    center *= 0.12; // mild damping

    // apply banking and center to uv
    vec2 q = bankRot * (uv - center);

    // base polar coords
    float r = length(q);
    float a = atan(q.y, q.x);

    // deep vortex warp: combine radial-dependent and angle-dependent warp
    float turnEase = easeInOut(fract(z / segLen));
    float baseWarp = 0.6 + 1.6 * clamp(warp, 0.0, 3.0); // user-controlled magnitude
    // radial falloff so center is more stable and outer walls twist strongly
    float warpFall = smoothstep(0.0, 1.6, r);
    // angular displacement: multi-frequency to create hammer-smear streaks
    a += baseWarp * turnEase * warpFall * (0.8 * sin(2.2 * a + 0.6*z) + 0.6 * sin(5.1 * a + 0.12*z));

    // Chromatic offset increases with warp to emphasize streaks
    float ca = (0.006 + 0.006 * turnEase) * (1.0 + 0.9 * clamp(warp, 0.0, 3.0));
    vec2 qR = bankRot * (uv - center) + vec2(ca, 0.0);
    vec2 qG = bankRot * (uv - center);
    vec2 qB = bankRot * (uv - center) - vec2(ca, 0.0);

    float rR = length(qR);
    float rG = length(qG);
    float rB = length(qB);
    float aR = atan(qR.y, qR.x);
    float aG = atan(qG.y, qG.x);
    float aB = atan(qB.y, qB.x);

    // strong rings and streaks - thickness controls softness
    float ringsR = ringsPattern(rR, z, thickness);
    float ringsG = ringsPattern(rG, z, thickness);
    float ringsB = ringsPattern(rB, z, thickness);

    vec3 colR = palette(aR, rR, z) * (0.5 + 0.6 * ringsR);
    vec3 colG = palette(aG, rG, z) * (0.5 + 0.6 * ringsG);
    vec3 colB = palette(aB, rB, z) * (0.5 + 0.6 * ringsB);

    vec3 col = vec3(colR.r, colG.g, colB.b);

    // intense inner streaks / motion lines: high frequency angular modulation
    float streak = smoothstep(0.0, 0.3, 1.0 - abs(sin(18.0 * (a + 0.2*z)) ) );
    col += 1.2 * vec3(0.9, 0.95, 1.0) * pow(max(0.0, 1.0 - r*6.0), 3.0) * streak * (0.5 + 0.8 * clamp(warp, 0.0, 3.0));

    // procedural hammer mask at axis (use unwarped local uv so hammer looks like object passing through)
    float hammer = hammerMask(uv * vec2(1.0, 1.6), z);
    // hammer glint and color (bright metal)
    vec3 hammerCol = mix(vec3(0.15,0.1,0.05), vec3(1.0,0.95,0.9), 0.9);
    // composite hammer onto col with additive glow to sell impact
    col = mix(col, hammerCol + 2.2 * vec3(1.0,0.9,0.6) * hammer, smoothstep(0.02, 0.6, hammer));

    // vignette and radial tone controlled by thickness
    float v = smoothstep(1.6, 0.2, r) * (0.6 + 0.4 * (1.5 - clamp(thickness, 0.2, 2.0)));
    col *= v;

    // final color shift (hue)
    if (abs(colorShift) > 1e-5) {
        col = hueShift(col, colorShift);
    }

    // clamp and gamma
    col = pow(clamp(col, 0.0, 1.0), vec3(0.9));

    FragColor = vec4(col, 1.0);
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
    GLint locTime = glGetUniformLocation(prog, "uTime");
    GLint locRes = glGetUniformLocation(prog, "uResolution");
    GLint locSpeed = glGetUniformLocation(prog, "uSpeed");
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
    glViewport(0, 0, w, h);
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
        std::cout << "Loc iTime=" << locTime
            << " uResolution=" << locRes
            << " speed=" << locSpeed
            << " warp=" << locWarp
            << " thickness=" << locThickness
            << " colorShift=" << locColorShift
            << "\n";
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