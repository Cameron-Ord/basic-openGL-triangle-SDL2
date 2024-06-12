#include <GL/glew.h>

#include <GL/gl.h>
#include <GL/glu.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>

#include <gsl/gsl_matrix.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct Matrix4x4 {
  float m[4][4];
};

struct SDLContextData {
  SDL_Window *w;
  SDL_GLContext *gl_context;
  int win_width;
  int win_height;
};

struct File {
  char *data;
  size_t len;
  int valid;
};

typedef struct SDLContextData SDLContextData;
typedef struct File File;
typedef struct Matrix4x4 Matrix4x4;

Matrix4x4 ortho;

float left = -1.0f;
float right = 1.0f;
float bottom = -1.0f;
float top = 1.0f;
float nearVal = -1.0f;
float farVal = 1.0f;

File file_io_read(char *path);
int create_shader(File *vertex, File *fragment);

#define IO_READ_CHUNK_SIZE 2097152

int main(int argc, char *argv[]) {

  // Not using this, but its a good thing to remember, although I think going
  // forward i will just use glm with c++

  ortho.m[0][0] = 2.0f / (right - left);
  ortho.m[0][1] = 0.0f;
  ortho.m[0][2] = 0.0f;
  ortho.m[0][3] = 0.0f;

  ortho.m[1][0] = 0.0f;
  ortho.m[1][1] = 2.0f / (top - bottom);
  ortho.m[1][2] = 0.0f;
  ortho.m[1][3] = 0.0f;

  ortho.m[2][0] = 0.0f;
  ortho.m[2][1] = 0.0f;
  ortho.m[2][2] = -2.0f / (farVal - nearVal);
  ortho.m[2][3] = 0.0f;

  ortho.m[3][0] = -(right + left) / (right - left);
  ortho.m[3][1] = -(top + bottom) / (top - bottom);
  ortho.m[3][2] = -(farVal + nearVal) / (farVal - nearVal);
  ortho.m[3][3] = 1.0f;

  SDLContextData context;

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    printf("%s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  context.w = SDL_CreateWindow("OpenGL example", SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, 800, 600,
                               SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  if (!context.w) {
    printf("%s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  context.gl_context = SDL_GL_CreateContext(context.w);
  if (!context.gl_context) {
    printf("%s\n", SDL_GetError());
    SDL_DestroyWindow(context.w);
    SDL_Quit();
  }

  SDL_GL_SetSwapInterval(1);

  GLenum err = glewInit();
  if (err != GLEW_OK) {
    printf("%s\n", SDL_GetError());
    SDL_DestroyWindow(context.w);
    SDL_GL_DeleteContext(context.gl_context);
    SDL_Quit();
    return 1;
  }

  printf("OpenGL vendor : %s\n", glGetString(GL_VENDOR));
  printf("OpenGL renderer : %s\n", glGetString(GL_RENDERER));
  printf("OpenGL version : %s\n", glGetString(GL_VERSION));

  File frag = file_io_read("def.frag");
  if (!frag.valid) {
    printf("failed to read file!\n");
    return 1;
  }

  File vert = file_io_read("def.vert");
  if (!vert.valid) {
    printf("failed to read file!\n");
    return 1;
  }

  uint32_t shader = create_shader(&vert, &frag);
  if (shader == 0) {
    printf("failed to create shader!\n");
    return 1;
  }

  int running = 1;

  float vertices[] = {
      0.0,  0.5,  0.0, // top right
      0.5,  -0.5, 0.0, // bottom right
      -0.5, -0.5, 0.0, // bottom left
      -0.0, 0.5,  0.0  // top left
  };
  unsigned int indices[] = {
      0, 1, 3, // first triangle
      1, 2, 3  // second triangle
  };

  unsigned int VBO, VAO, EBO;

  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glGenBuffers(1, &EBO);

  glBindVertexArray(VAO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
               GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  while (running) {

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
      case SDL_QUIT: {
        running = 0;
        break;
      }
      }
    }

    glUseProgram(shader);
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    SDL_GL_SwapWindow(context.w);
    SDL_Delay(16);
  }

  SDL_Quit();
  return 0;
}

File file_io_read(char *path) {
  File file = {.data = NULL, .len = 0, .valid = 0};

  FILE *fp = fopen(path, "rb");
  if (ferror(fp)) {
    return file;
  }

  char *data = NULL;
  char *tmp;

  size_t used = 0;
  size_t size = 0;
  size_t n;

  while (1) {
    if (used + IO_READ_CHUNK_SIZE + 1 > size) {
      size = used + IO_READ_CHUNK_SIZE + 1;

      if (size <= used) {
        free(data);
        return file;
      }

      tmp = realloc(data, size);
      if (!tmp) {
        free(data);
        return file;
      }
      data = tmp;
    }
    n = fread(data + used, 1, IO_READ_CHUNK_SIZE, fp);
    if (n == 0) {
      break;
    }
    used += n;
  }

  if (ferror(fp)) {
    free(data);
    return file;
  }

  tmp = realloc(data, used + 1);
  if (!tmp) {
    free(data);
    return file;
  }

  data = tmp;
  data[used] = 0;

  file.data = data;
  file.len = used;
  file.valid = 1;

  return file;
}

int create_shader(File *vertex, File *fragment) {
  int success;
  char log[512];

  uint32_t shader_vertex = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(shader_vertex, 1, (const char *const *)vertex, NULL);
  glCompileShader(shader_vertex);
  glGetShaderiv(shader_vertex, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(shader_vertex, 512, NULL, log);
    printf("%s\n", log);
    return 0;
  }

  uint32_t shader_fragment = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(shader_fragment, 1, (const char *const *)fragment, NULL);
  glCompileShader(shader_fragment);
  glGetShaderiv(shader_fragment, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(shader_fragment, 512, NULL, log);
    printf("%s\n", log);
    return 0;
  }

  uint32_t shader = glCreateProgram();
  glAttachShader(shader, shader_vertex);
  glAttachShader(shader, shader_fragment);
  glLinkProgram(shader);

  free(vertex->data);
  free(fragment->data);

  return shader;
}
