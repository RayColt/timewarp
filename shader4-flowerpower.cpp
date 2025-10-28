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

uniform vec2 iResolution;
uniform float iTime;

void main() {
    vec2 p = (gl_FragCoord.xy * 2.0 - iResolution.xy) / iResolution.y;
    float r = length(p);
    float a = atan(p.y, p.x);
    float z = iTime * 2.0;

    float rings = sin(10.0*r - z);
    float stripes = sin(6.0*a + z);
    float v = rings * stripes;

    vec3 col = vec3(0.5+0.5*v, 0.3+0.3*v, 0.8-0.5*v);
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