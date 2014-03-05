#ifndef GL_GL_H
#define GL_GL_H

#if __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <GLUT/glut.h>
#elif WIN32
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <GL/glext.h>
#elif USAR_OPENGL_ES
#include <GLES/gl.h>
#include <GLES/glext.h>
//#include <GLES/egl.h>  Da problema com o simbolo None definido no X11/X.h, uma enum do Qt em qstyleoption.h usa None tambem.
#include <GLES/glplatform.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define RAD_PARA_GRAUS (180.0f / M_PI)
#define GRAUS_PARA_RAD (M_PI / 180.0f)

namespace gl {

// Inicializacao e finalizacao da parte grafica.
void IniciaGl(int* argcp, char** argv);
void FinalizaGl();

class Contexto{
 public:
  Contexto(int* argcp, char** argv) { IniciaGl(argcp, argv); }
  ~Contexto() { FinalizaGl(); }
};

/** Salva a matriz corrente durante escopo da classe. Ou muda o modo de matriz e a salva, retornando ao modo anterior ao fim do escopo. */
class MatrizEscopo {
 public:
  /** Salva a matriz corrente pelo escopo. */
  MatrizEscopo() : modo_anterior_(GL_INVALID_ENUM), modo_(GL_INVALID_ENUM) { glPushMatrix(); }

  /** Muda matriz para matriz do modo e a salva pelo escopo. */
  explicit MatrizEscopo(GLenum modo) : modo_(modo) {
    glGetIntegerv(GL_MATRIX_MODE, (GLint*)&modo_anterior_);
    glMatrixMode(modo_);
    glPushMatrix();
  }

  /** Restaura matriz anterior ao escopo para o modo escolhido. */
  ~MatrizEscopo() {
    if (modo_ != GL_INVALID_ENUM) {
      glMatrixMode(modo_);
    }
    glPopMatrix();
    if (modo_anterior_ != GL_INVALID_ENUM) {
      glMatrixMode(modo_anterior_);
    }
  }

 private:
  GLenum modo_anterior_;
  // O valor GL_INVALID_ENUM indica que nao eh para restaurar a matriz.
  GLenum modo_;
};

/** Habilita uma caracteristica pelo escopo. Ver glEnable. */
class HabilitaEscopo {
 public:
  HabilitaEscopo(GLenum cap) : cap_(cap) { glEnable(cap_); }
  ~HabilitaEscopo() { glDisable(cap_); }
 private:
  GLenum cap_;
};

/** Desabilita uma caracteristica pelo escopo. Ver glDisable. */
class DesabilitaEscopo {
 public:
  DesabilitaEscopo(GLenum cap) : cap_(cap) { glDisable(cap_); }
  ~DesabilitaEscopo() { glEnable(cap_); }
 private:
  GLenum cap_;
};

class ModeloLuzEscopo {
 public:
  ModeloLuzEscopo(const GLfloat* luz) { glGetFloatv(GL_LIGHT_MODEL_AMBIENT, luz_antes); glLightModelfv(GL_LIGHT_MODEL_AMBIENT, luz); }
  ~ModeloLuzEscopo() { glLightModelfv(GL_LIGHT_MODEL_AMBIENT, luz_antes); }
 private:
  GLfloat luz_antes[4];
};

#if !USAR_OPENGL_ES
/* glPush/PopAttrib bits */
enum bits_atributos_e {
  BIT_POLIGONO = GL_POLYGON_BIT,
  BIT_LUZ = GL_LIGHTING_BIT,
  BIT_PROFUNDIDADE = GL_DEPTH_BUFFER_BIT,
  BIT_STENCIL = GL_STENCIL_BUFFER_BIT,
  BIT_HABILITAR = GL_ENABLE_BIT,
  BIT_COR = GL_COLOR_BUFFER_BIT,
};
/** Salva os atributos durante o escopo da variavel, restaurando no fim. */
class AtributosEscopo {
 public:
  AtributosEscopo(unsigned int mascara) { glPushAttrib(mascara); }
  ~AtributosEscopo() { glPopAttrib(); }
};
#endif

/** Funcoes gerais. */
inline void Le(GLenum nome_parametro, GLint* valor) { glGetIntegerv(nome_parametro, valor); }
inline void Le(GLenum nome_parametro, GLfloat* valor) { glGetFloatv(nome_parametro, valor); }
#if !USAR_OPENGL_ES
inline void Le(GLenum nome_parametro, GLdouble* valor) { glGetDoublev(nome_parametro, valor); }
#endif
inline void Habilita(GLenum cap) { glEnable(cap); }
inline void Desabilita(GLenum cap) { glDisable(cap); }
inline void HabilitaEstadoCliente(GLenum cap) { glEnableClientState(cap); }
inline void DesabilitaEstadoCliente(GLenum cap) { glDisableClientState(cap); }
inline void DesvioProfundidade(GLfloat fator, GLfloat unidades) { glPolygonOffset(fator, unidades);  }
inline void CarregaIdentidade() { glLoadIdentity(); }
inline void MultiplicaMatriz(const GLfloat* matriz) { glMultMatrixf(matriz); }
inline void ModoMatriz(GLenum modo) { glMatrixMode(modo); }
#if !USAR_OPENGL_ES
inline void EmpilhaAtributo(GLbitfield mascara) { glPushAttrib(mascara); }
inline void DesempilhaAtributo() { glPopAttrib(); }
#else
#endif

/** Desenha elementos e afins. */
inline void DesenhaElementos(GLenum modo, GLsizei num_vertices, GLenum tipo, const GLvoid* indices) {
  glDrawElements(modo, num_vertices, tipo, indices);
}
inline void PonteiroVertices(GLint vertices_por_coordenada, GLenum tipo, const GLvoid* vertices) {
  glVertexPointer(vertices_por_coordenada, tipo, 0, vertices);
}
inline void PonteiroVerticesTexturas(GLint vertices_por_coordenada, GLenum tipo, const GLvoid* vertices) {
  glTexCoordPointer(vertices_por_coordenada, tipo, 0, vertices);
}
inline void PonteiroNormais(GLenum tipo, const GLvoid* normais) { glNormalPointer(tipo, 0, normais);  }
#if USAR_OPENGL_ES
void Retangulo(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
#else
inline void Retangulo(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2) {
  glRectf(x1, y1, x2, y2);
}
#endif

/** Funcoes de nomes. */
#if !USAR_OPENGL_ES
inline void IniciaNomes() { glInitNames(); }
inline void EmpilhaNome(GLuint nome) { glPushName(nome); }
inline void CarregaNome(GLuint nome) { glLoadName(nome); }
inline void DesempilhaNome() { glPopName(); }
#else
void IniciaNomes();
void EmpilhaNome(GLuint nome);
void CarregaNome(GLuint nome);
void DesempilhaNome();
#endif

/** Empilha o nome no inicio do escopo e desempilha no final. */
class NomesEscopo {
 public:
  NomesEscopo(GLuint nome) { EmpilhaNome(nome); }
  ~NomesEscopo() { DesempilhaNome(); }
};

/** Funcoes de escala, translacao e rotacao. */
inline void Escala(GLfloat x, GLfloat y, GLfloat z) { glScalef(x, y, z); }
inline void Translada(GLfloat x, GLfloat y, GLfloat z) { glTranslatef(x, y, z); }
inline void Roda(GLfloat angulo_graus, GLfloat x, GLfloat y, GLfloat z) { glRotatef(angulo_graus, x, y, z); }

/** Funcoes de iluminacao. */
inline void Luz(GLenum luz, GLenum nome_param, GLfloat param) { glLightf(luz, nome_param, param); }
inline void Luz(GLenum luz, GLenum nome_param, const GLfloat* params) { glLightfv(luz, nome_param, params); }
inline void ModeloLuz(GLenum nome_param, const GLfloat* params) { glLightModelfv(nome_param, params); }

/** Funcoes de normais. */
inline void Normal(GLfloat x, GLfloat y, GLfloat z) { glNormal3f(x, y, z); }

/** Objetos GLU e GLUT. */
void CilindroSolido(GLfloat raio_base, GLfloat raio_topo, GLfloat altura, GLint fatias, GLint tocos);
void ConeSolido(GLfloat base, GLfloat altura, GLint num_fatias, GLint num_tocos);
void EsferaSolida(GLfloat raio, GLint num_fatias, GLint num_tocos);
void CuboSolido(GLfloat tam_lado);

/** Raster. */
#if !USAR_OPENGL_ES
inline void PosicaoRaster(GLfloat x, GLfloat y, GLfloat z) { glRasterPos3f(x, y, z); }
inline void PosicaoRaster(GLint x, GLint y) { glRasterPos2i(x, y); }
inline void DesenhaCaractere(char c) { glutBitmapCharacter(GLUT_BITMAP_8_BY_13, c); }
#else
inline void PosicaoRaster(GLfloat x, GLfloat y, GLfloat z) { /* TODO */ }
inline void PosicaoRaster(GLint x, GLint y) { /* TODO */ }
inline void DesenhaCaractere(char c) { /* TODO */ }
#endif

/** Matriz de olho e perspectiva e picking. */
#if !USAR_OPENGL_ES
inline void Perspectiva(GLdouble angulo_y, GLdouble aspecto, GLdouble z_perto, GLdouble z_longe) {
  gluPerspective(angulo_y, aspecto, z_perto, z_longe);
}
inline void OlharPara(GLdouble olho_x, GLdouble olho_y, GLdouble olho_z,
               GLdouble centro_x, GLdouble centro_y, GLdouble centro_z,
               GLdouble cima_x, GLdouble cima_y, GLdouble cima_z) {
  gluLookAt(olho_x, olho_y, olho_z, centro_x, centro_y, centro_z, cima_x, cima_y, cima_z);
}
inline void Ortogonal(GLdouble esquerda, GLdouble direita, GLdouble baixo, GLdouble cima, GLdouble proximo, GLdouble distante) {
  glOrtho(esquerda, direita, baixo, cima, proximo, distante);
}
inline GLint Desprojeta(GLdouble x_janela, GLdouble y_janela, GLdouble profundidade_3d,
                        const GLdouble* model, const GLdouble* proj, const GLint* view,
                        GLfloat* x3d, GLfloat* y3d, GLfloat* z3d) {
  double x3dd, y3dd, z3dd;
  GLint ret = gluUnProject(x_janela, y_janela, profundidade_3d, model, proj, view, &x3dd, &y3dd, &z3dd);
  *x3d = x3dd; *y3d = y3dd; *z3d = z3dd;
  return ret;
}
inline void MatrizPicking(GLdouble x, GLdouble y, GLdouble delta_x, GLdouble delta_y, GLint *viewport) {
  gluPickMatrix(x, y, delta_x, delta_y, viewport);
}
#else
void Perspectiva(float angulo_y, float aspecto, float z_perto, float z_longe);
void OlharPara(float olho_x, float olho_y, float olho_z,
               float centro_x, float centro_y, float centro_z,
               float cima_x, float cima_y, float cima_z);
inline void Ortogonal(float esquerda, float direita, float baixo, float cima, float proximo, float distante) {
  glOrthof(esquerda, direita, baixo, cima, proximo, distante);
}
GLint Desprojeta(float x_janela, float y_janela, float profundidade_3d,
                 const float* model, const float* proj, const GLint* view,
                 GLfloat* x3d, float* y3d, float* z3d);
void MatrizPicking(float x, float y, float delta_x, float delta_y, GLint *viewport);
#endif

/** Picking. */
#if !USAR_OPENGL_ES
enum modo_renderizacao_e {
  MR_RENDER = GL_RENDER,
  MR_SELECT = GL_SELECT
};
inline GLint ModoRenderizacao(modo_renderizacao_e modo) { return glRenderMode(modo); }
inline void BufferSelecao(GLsizei tam_buffer, GLuint* buffer) { glSelectBuffer(tam_buffer, buffer); }
#else
enum modo_renderizacao_e {
  MR_RENDER = 0x1C00,
  MR_SELECT = 0x1C02
};
/* Render Mode */
GLint ModoRenderizacao(modo_renderizacao_e modo);
void BufferSelecao(GLsizei tam_buffer, GLuint* buffer);
#endif

/** Mudanca de cor. */
#if !USAR_OPENGL_ES
inline void MudaCor(float r, float g, float b, float a) {
  GLfloat cor[4] = { r, g, b, a };
  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, cor);
  glColor4f(r, g, b, a);
}
#else
void MudaCor(float r, float g, float b, float a);
#endif

}  // namespace gl


#endif  // GL_GL_H
