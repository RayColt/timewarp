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
in vec2 uv;
out vec4 fragColor;
uniform float iTime;
uniform vec2 iResolution;
uniform float speed;
uniform float warp;
uniform float thickness;
uniform float colorShift;

// 2D hash / noise
float hash21(vec2 p){
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}
float noise(vec2 p){
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f*f*(3.0-2.0*f);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0,0.0));
    float c = hash21(i + vec2(0.0,1.0));
    float d = hash21(i + vec2(1.0,1.0));
    return mix(mix(a,b,f.x), mix(c,d,f.x), f.y);
}

// palette
vec3 palette(float t){
    // shifted triadic palette
    float r = 0.5 + 0.5 * sin(6.28318*(t + 0.00 + colorShift));
    float g = 0.5 + 0.5 * sin(6.28318*(t + 0.33 + colorShift));
    float b = 0.5 + 0.5 * sin(6.28318*(t + 0.66 + colorShift));
    return vec3(r,g,b);
}

// tunnel SDF: distance to tubular oscillating surface
float tunnelSDF(vec3 p){
    // convert to cylindrical coords
    float r = length(p.xy);
    // add longitudinal waves to radius
    float wave = 0.3 * sin(6.0 * p.z + 2.0 * sin(3.0 * p.z + iTime * 0.6));
    float rings = 0.2 * sin(40.0 * (r + 0.5 * sin(2.0 * p.z + iTime)));
    float radius = 1.0 + wave + rings;
    return r - radius;
}

void main(){
    vec2 p = (uv * 2.0 - 1.0);
    p.x *= iResolution.x / iResolution.y;

    // camera ray
    vec3 ro = vec3(0.0, 0.0, iTime * speed);
    vec3 rd = normalize(vec3(p.xy, -1.5 + 0.3 * sin(iTime*0.2))); // slight pitch oscillation

    // march along ray; sample tunnel SDF (signed distance)
    float t = 0.0;
    float glow = 0.0;
    float accum = 0.0;
    float thicknessLocal = thickness;
    for(int i=0;i<120;i++){
        vec3 pos = ro + rd * t;
        // make tunnel repeat in z so it looks infinite
        float zWrapped = mod(pos.z, 12.566370); // 2*pi*2 approximated
        vec3 rp = vec3(pos.xy, zWrapped);

        float d = tunnelSDF(rp);
        // soft surface response
        float hit = exp(-20.0 * abs(d)); // soft band
        // plasma layers and noise
        float n = noise(vec2(pos.x*1.3 + iTime*0.5, pos.y*1.3 - iTime*0.3));
        float layer = 0.5 + 0.5 * sin(8.0 * pos.z + 3.0*n + iTime*2.0);
        accum += hit * layer;
        glow += hit * (1.0 - smoothstep(0.0, thicknessLocal, abs(d)));

        // adaptive step: small near surface, larger in emptier space
        t += max(0.02, 0.5 * abs(d));
        if(t > 100.0) break;
    }

    // color by accum and depth
    float depth = clamp(exp(-0.02 * t), 0.0, 1.0);
    float intensity = clamp(accum * 0.6 + glow * 0.8, 0.0, 2.5);

    // multicolored palette using depth and longitudinal harmonics
    float palettePos = fract((iTime * 0.1 * warp) + (t * 0.02) + accum*0.1);
    vec3 col = palette(palettePos) * intensity;

    // add radial streaks / plasma veins
    float veins = 0.5 + 0.5 * sin(20.0 * length(p) - iTime * 2.5 + noise(p*10.0));
    col += 0.15 * palette(palettePos + 0.2) * veins;

    // vignetting and fog
    float vig = smoothstep(1.2, 0.2, length(p));
    col *= vig;
    col = mix(vec3(0.02,0.02,0.03), col, depth);

    // gamma
    col = pow(clamp(col, 0.0, 1.0), vec3(0.8));
    fragColor = vec4(col, 1.0);
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