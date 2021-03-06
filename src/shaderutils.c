/**
 * From the OpenGL Programming wikibook:
 * http://en.wikibooks.org/wiki/OpenGL_Programming
 * This file is in the public domain.
 * Contributors: Sylvain Beucler
 */

#include "shaderutils.h"
#include "results.h"

int
sls_get_glversion()
{
#ifndef __EMSCRIPTEN__
  int major, minor;
  glGetIntegerv(GL_MAJOR_VERSION, &major);
  glGetIntegerv(GL_MINOR_VERSION, &minor);
  return major * 100 + minor * 10;

#else
  return 300;
#endif
}

/**
 * Store all the file's contents in memory, useful to pass shaders
 * source code to OpenGL
 */
char*
sls_file_read(const char* filename)
{
  FILE* file = fopen(filename, "rb");
  if (file == NULL)
    return NULL;

  int file_size = BUFSIZ;
  char* file_str = (char*)malloc((size_t)file_size);
  int nb_read_total = 0;

  while (!feof(file) && !ferror(file)) {
    if (nb_read_total + BUFSIZ > file_size) {
      if (file_size > 10 * 1024 * 1024)
        break;
      file_size = file_size * 2;
      file_str = (char*)realloc(file_str, (size_t)file_size);
    }
    char* p_res = file_str + nb_read_total;
    nb_read_total += fread(p_res, 1, BUFSIZ, file);
  }

  fclose(file);
  file_str = (char*)realloc(file_str, nb_read_total + 1);
  file_str[nb_read_total] = '\0';
  return file_str;
}

/**
 * Display compilation errors from the OpenGL shader compiler
 */
void
_sls_print_log(GLuint object,
               slsGLObjectType type,
               char const* file,
               char const* func,
               long line)
{
  if (file && func) {
    sls_log_err("Shader Error: %s, %s: %li:", file, func, line);
  }
  GLint log_length = 0;
  if (type == SLS_TYPE_SHADER) {
    glGetShaderiv(object, GL_INFO_LOG_LENGTH, &log_length);
  } else if (type == SLS_TYPE_PROGRAM) {
    glGetProgramiv(object, GL_INFO_LOG_LENGTH, &log_length);
  } else {
    sls_log_err("Not a active_shader or a program\n");
    return;
  }

  char* log = (char*)malloc((size_t)log_length);

  if (type == SLS_TYPE_SHADER) {
    glGetShaderInfoLog(object, log_length, NULL, log);
  } else if (type == SLS_TYPE_PROGRAM) {
    glGetProgramInfoLog(object, log_length, NULL, log);
  }

  sls_log_err("%s\n", log);
  free(log);
}

/**
 * Compile the shader from file 'filename', with error handling
 */
GLuint
sls_create_shader(slsResultCode* res_out, const char* source, GLenum type)
{
  GLchar const* modern_preamble = "#version 330 core\n";
  GLchar const* gles_preamble = "#version 300 es\n";

  GLchar const* preamble;

#ifndef __EMSCRIPTEN__
  preamble = modern_preamble;
#else
  preamble = gles_preamble;
#endif

  sls_set_result(res_out, SLS_OK);

  GLuint res = glCreateShader(type);

  char const* sources[] = { preamble, source };
  const size_t n_sources = sizeof(sources) / sizeof(char*);

  glShaderSource(res, (GLsizei)n_sources, sources, NULL);

  glCompileShader(res);
  GLint compile_ok = GL_FALSE;
  glGetShaderiv(res, GL_COMPILE_STATUS, &compile_ok);

  if (compile_ok == GL_FALSE) {
    sls_log_info("source: %s%s\n", preamble, source);
    sls_print_log(res, SLS_TYPE_SHADER);
    glDeleteShader(res);
    sls_set_result(res_out, SLS_COMPILE_FAILED);
    return 0;
  }


  return res;
}

GLuint
_sls_link_program(slsResultCode* result_out,
                  GLuint vertex,
                  GLuint frag,
                  GLuint geom,
                  bool has_geometry_shader,
                  GLint gs_input,
                  GLint gs_output,
                  GLint gs_vertices)
{
  GLuint program = glCreateProgram();
  int link_ok;

  if (!glIsShader(vertex) || !glIsShader(frag)) {
    sls_set_result(result_out, SLS_ERROR);
    sls_log_err("not a shader object");
    return 0;
  }
  glAttachShader(program, vertex);
  glAttachShader(program, frag);
#ifndef __EMSCRIPTEN__ // no geometry shaders in webgl
  if (has_geometry_shader) {
    glAttachShader(program, geom);

    glProgramParameteri(program, GL_GEOMETRY_INPUT_TYPE, gs_input);
    glProgramParameteri(program, GL_GEOMETRY_OUTPUT_TYPE, gs_output);
    glProgramParameteri(program, GL_GEOMETRY_VERTICES_OUT, gs_vertices);
  }

#endif

  glLinkProgram(program);

  glGetProgramiv(program, GL_LINK_STATUS, &link_ok);
  if (!link_ok) {
    sls_print_log(program, SLS_TYPE_PROGRAM);
    GLenum err;
    int n = 0;
    while ((err = glGetError()) != GL_NO_ERROR) {
      n++;
      sls_log_err("#%d: gl error 0x%x", n, err);
    }
    glDeleteProgram(program);
    sls_set_result(result_out, SLS_LINK_FAILED);
    return 0;
  }

  sls_set_result(result_out, SLS_OK);
  return program;
}

char*
sls_file_dumps(char const* rootdir, char const* path)
{
  char *full_path = NULL;
  bool full_path_alloced = false;
  if (rootdir) {
    full_path_alloced = true;
    size_t full_path_len = strlen(rootdir) + strlen(path);
    full_path = calloc(full_path_len + 1, sizeof(char));
    char const *path_start = path;
    if (path_start[0] == '.' && path_start[1] == '/') {
      path_start += 2;
    }
    size_t offset = SDL_strlcpy(full_path, rootdir, full_path_len);
    strncat(full_path + offset - 2, path_start, full_path_len - offset);
  } else {
    full_path = (char*) path;
  }

  char* buffer = NULL;
  FILE* file = NULL;
  file = fopen(path, "rb");
  sls_check(file, "file not found: \"%s\"", path);
  long start = ftell(file);
  sls_check(0 == fseek(file, 0, SEEK_END), "could not seek end of file");
  long end = ftell(file);
  size_t alloc_size = (size_t)end - start;
  buffer = malloc(alloc_size);
  buffer[alloc_size - 1] = '\0';
  rewind(file);
  fread(buffer, sizeof(char), alloc_size - 1, file);
  buffer[alloc_size - 1] = '\0';
  if (full_path_alloced) {
    free(full_path);
  }
  fclose(file);

  return buffer;
error:
  if (full_path_alloced) {
    free(full_path);
  }
  if (file) {
    fclose(file);
  }
  if (buffer) {
    free(buffer);
  }
  return NULL;
}

#include <Windows.h>
GLuint
sls_shader_from_source(slsResultCode* res_out,
                       char const* path,
                       GLenum shader_type)
{
  slsResultCode tmp = SLS_OK;
  if (!res_out) {
    res_out = &tmp;
  }
  GLuint shader = 0;
  sls_set_result(res_out, SLS_ERROR);
  char buffer[255];
  GetCurrentDirectoryA(255, buffer);
  sls_log_info("pwd: %s", buffer);
  char* source = NULL;
  source = sls_file_dumps(buffer, path);
  sls_check(source, "could not read file %s", path);
  shader = sls_create_shader(res_out, source, shader_type);
  sls_check( *res_out == SLS_OK, "could not compile shader");

  free(source);
  sls_set_result(res_out, SLS_OK);
  return shader;
error:
  if (source) {
    free(source);
  }
  return 0;
}
