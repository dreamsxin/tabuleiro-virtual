#define NOVO_DESENHO 2
#if 0 && !USAR_OPENGL_ES
#include "gltab/gl.h"

namespace gl {

void IniciaGl(int* argcp, char** argv) {
  glutInit(argcp, argv);
}
void FinalizaGl() {
}

// OpenGL normal.
void ConeSolido(GLfloat base, GLfloat altura, GLint num_fatias, GLint num_tocos) {
  glutSolidCone(base, altura, num_fatias, num_tocos);
}

void EsferaSolida(GLfloat raio, GLint num_fatias, GLint num_tocos) {
  glutSolidSphere(raio, num_fatias, num_tocos);
}

void CuboSolido(GLfloat tam_lado) {
  glutSolidCube(tam_lado);
}

void DesenhaString(const std::string& str) {
  // Le o raster em coordenadas de janela.
  GLint raster_pos[4];
  glGetIntegerv(GL_CURRENT_RASTER_POSITION, raster_pos);
  // Le viewport.
  GLint viewport[4];
  gl::Le(GL_VIEWPORT, viewport);
  int largura = viewport[2], altura = viewport[3];

  // Muda para projecao 2D.
  gl::MatrizEscopo salva_matriz_2(GL_PROJECTION);
  gl::CarregaIdentidade();
  gl::Ortogonal(0, largura, 0, altura, 0, 1);
  gl::MatrizEscopo salva_matriz_3(GL_MODELVIEW);
  gl::CarregaIdentidade();
  glRasterPos2i(raster_pos[0] - (str.size() / 2) * 8, raster_pos[1]);
  for (const char c : str) {
    gl::DesenhaCaractere(c);
  }
}


void CilindroSolido(GLfloat raio_base, GLfloat raio_topo, GLfloat altura, GLint fatias, GLint tocos) {
  GLUquadric* cilindro = gluNewQuadric();
  gluQuadricOrientation(cilindro, GLU_OUTSIDE);
  gluQuadricNormals(cilindro, GLU_SMOOTH);
  gluQuadricDrawStyle(cilindro, GLU_FILL);
  gluCylinder(cilindro, raio_base, raio_topo, altura, fatias, tocos);
  gluDeleteQuadric(cilindro);
}

}  // namespace gl.

#else
// OpenGL ES.
// Varias funcoes copiadas do GLUES: https://code.google.com/p/glues/.

#include <cmath>
#include <unordered_map>
#include <utility>
#include <vector>
#include "gltab/gl.h"
#include "log/log.h"

namespace gl {

#define __glPi 3.14159265358979323846
namespace {

// Atlas de caracteres. Aqui estao vertices usados para construi-los. Cada linha eh um indice.
const static std::vector<float> g_vertices_caracteres = {
  0.1f, 0.1f,  // 0
  0.1f, 0.2f,  // 1
  0.1f, 0.8f,  // 2
  0.1f, 0.9f,  // 3
  0.2f, 0.1f,  // 4
  0.2f, 0.2f,  // 5
  0.2f, 0.8f,  // 6
  0.2f, 0.9f,  // 7
  0.8f, 0.1f,  // 8
  0.8f, 0.2f,  // 9
  0.8f, 0.8f,  // 10
  0.8f, 0.9f,  // 11
  0.9f, 0.1f,  // 12
  0.9f, 0.2f,  // 13
  0.9f, 0.8f,  // 14
  0.9f, 0.9f,  // 15
  0.45f, 0.1f, // 16
  0.45f, 0.2f, // 17
  0.45f, 0.8f, // 18
  0.45f, 0.9f, // 19
  0.55f, 0.1f, // 20
  0.55f, 0.2f, // 21
  0.55f, 0.8f, // 22
  0.55f, 0.9f, // 23
  0.45f, 0.5f, // 24
  0.55f, 0.5f, // 25
  0.9f, 0.35f, // 26
  0.1f, 0.35f, // 27
  0.1f, 0.45f, // 28
  0.1f, 0.55f, // 29
  0.2f, 0.45f, // 30
  0.2f, 0.55f, // 31
  0.8f, 0.45f, // 32
  0.8f, 0.55f, // 33
  0.9f, 0.45f, // 34
  0.9f, 0.55f, // 35
};

const static std::unordered_map<char, std::vector<short>> g_indices_caracteres = {
  {
    '0', {
      4, 8, 9, 9, 5, 4,  // S
      9, 13, 14, 14, 10, 9, // L
      10, 7, 6, 10, 11, 7,  // N
      5, 6, 2, 2, 1, 5,  // O
      4, 5, 1, // SO
      8, 13, 9, // SE
      10, 14, 11, // NE
      2, 6, 7,  // NO
    },
  }, {
    '1', {
      6, 22, 23,  // N
      17, 21, 22, 22, 18, 17,  // Meio
      4, 8, 9, 9, 5, 4,  // Base
    },
  }, {
    '2', {
      6, 22, 23, 22, 14, 23,  // N
      0, 4, 14, 0, 14, 10,  // Meio
      0, 12, 13, // Base
    },
  }, {
    '3', {
      6, 22, 23, 22, 10, 23,  // N
      24, 25, 10, 24, 10, 22,  // NE
      9, 13, 24, 13, 25, 24,  // SE
      20, 13, 1,  // Base
    },
  }, {
    '4', {
      5, 17, 19,  // O
      5, 13, 26,  // Base
      20, 8, 11,  // L
    },
  }, {
    '5', {
      6, 14, 15, 15, 7, 6,  // N
      30, 24, 7,  // NO
      30, 34, 24, 24, 34, 33,  // Meio
      13, 34, 32, 13, 32, 9,  // L
      0, 12, 13, 0, 13, 1,  // S
    },
  }, {
    '6', {
      2, 14, 19,  // N
      1, 5, 2, 5, 6, 2,  // O
      4, 5, 1, 4, 8, 9, 9, 5, 4, 8, 13, 9,  // S
      9, 13, 34, 34, 32, 9, 32, 34, 33,  // L
      30, 32, 33, 30, 33, 31,  // Meio.
    },
  }, {
    '7', {
      6, 14, 15, 15, 7, 6,  // N
      4, 14, 10, 4, 16, 14,  // Corpo
    },
  }, {
    '8', {
      6, 18, 19, 18, 22, 23, 18, 23, 19, 22, 10, 23, // N
      31, 18, 6, 30, 32, 33, 33, 31, 30, 33, 10, 22, // Meio
      31, 1, 4, 4, 8, 9, 9, 5, 4, 8, 13, 33,  // Base
    },
  }, {
    '9', {
      2, 6, 7, 6, 10, 7, 10, 11, 7, 10, 14, 11,  // N
      8, 12, 14, 14, 10, 8,  // L
      28, 30, 2, 30, 6, 2,  // O
      30, 32, 33, 33, 31, 30,  // Meio
    },
  },
};

void PreencheIdentidade(GLfloat m[16]) {
  m[0+4*0] = 1; m[0+4*1] = 0; m[0+4*2] = 0; m[0+4*3] = 0;
  m[1+4*0] = 0; m[1+4*1] = 1; m[1+4*2] = 0; m[1+4*3] = 0;
  m[2+4*0] = 0; m[2+4*1] = 0; m[2+4*2] = 1; m[2+4*3] = 0;
  m[3+4*0] = 0; m[3+4*1] = 0; m[3+4*2] = 0; m[3+4*3] = 1;
}

void Normaliza(GLfloat v[3]) {
  GLfloat r = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (r == 0.0f) {
    return;
  }
  v[0] /= r;
  v[1] /= r;
  v[2] /= r;
}

void ProdutoVetorial(GLfloat v1[3], GLfloat v2[3], GLfloat result[3]) {
  result[0] = v1[1] * v2[2] - v1[2] * v2[1];
  result[1] = v1[2] * v2[0] - v1[0] * v2[2];
  result[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

/** Matriz de rotacao baseada no eixo X. */
void MatrizRotacaoX(GLfloat angulo, GLfloat m[16]) {
  float s = sinf(angulo * GRAUS_PARA_RAD);
  float c = cosf(angulo * GRAUS_PARA_RAD);
  m[0+4*0] = 1; m[0+4*1] = 0; m[0+4*2] = 0;  m[0+4*3] = 0;
  m[1+4*0] = 0; m[1+4*1] = c; m[1+4*2] = -s; m[1+4*3] = 0;
  m[2+4*0] = 0; m[2+4*1] = s; m[2+4*2] = c;  m[2+4*3] = 0;
  m[3+4*0] = 0; m[3+4*1] = 0; m[3+4*2] = 0;  m[3+4*3] = 1;
}

/** Matriz de rotacao baseada no eixo X. */
void MatrizRotacaoY(GLfloat angulo, GLfloat m[16]) {
  float s = sinf(angulo * GRAUS_PARA_RAD);
  float c = cosf(angulo * GRAUS_PARA_RAD);
  m[0+4*0] = c;  m[0+4*1] = 0; m[0+4*2] = 0;  m[0+4*3] = 0;
  m[1+4*0] = 0;  m[1+4*1] = 1; m[1+4*2] = -s; m[1+4*3] = 0;
  m[2+4*0] = -s; m[2+4*1] = 0; m[2+4*2] = c;  m[2+4*3] = 0;
  m[3+4*0] = 0;  m[3+4*1] = 0; m[3+4*2] = 0;  m[3+4*3] = 1;
}

/** Matriz de rotacao em Z. */
void MatrizRotacaoZ(GLfloat angulo, GLfloat m[16]) {
  float s = sinf(angulo * GRAUS_PARA_RAD);
  float c = cosf(angulo * GRAUS_PARA_RAD);
  m[0+4*0] = c; m[0+4*1] = -s; m[0+4*2] = 0; m[0+4*3] = 0;
  m[1+4*0] = s; m[1+4*1] = c;  m[1+4*2] = 0; m[1+4*3] = 0;
  m[2+4*0] = 0; m[2+4*1] = 0;  m[2+4*2] = 1; m[2+4*3] = 0;
  m[3+4*0] = 0; m[3+4*1] = 0;  m[3+4*2] = 0; m[3+4*3] = 1;
}

/* Multiplica a matriz openGL matriz pelo vetor. A matriz OpenGL tem formato col x linha (column major), portanto,
* ao inves de multiplicar matriz (4x4) pelo vetor (4x1), fazemos a inversao: vetor (1x4) pela matriz (4x4).
*/
void MultiplicaMatrizVetor(const float m[16], float vetor[4]) {
  GLfloat res[4];
  res[0] = vetor[0] * m[0] + vetor[1] * m[4] + vetor[2] * m[8]  + vetor[3] * m[12];
  res[1] = vetor[0] * m[1] + vetor[1] * m[5] + vetor[2] * m[9]  + vetor[3] * m[13];
  res[2] = vetor[0] * m[2] + vetor[1] * m[6] + vetor[2] * m[10] + vetor[3] * m[14];
  res[3] = vetor[0] * m[3] + vetor[1] * m[7] + vetor[2] * m[11] + vetor[3] * m[15];
  memcpy(vetor, res, sizeof(res));
}

/*
** Invert 4x4 matrix.
** Contributed by David Moore (See Mesa bug #6748)
*/
static int __gluInvertMatrixf(const GLfloat m[16], GLfloat invOut[16])
{
    GLfloat inv[16], det;
    int i;

    inv[0] =   m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15]
             + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
    inv[4] =  -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15]
             - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
    inv[8] =   m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15]
             + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
    inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14]
             - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
    inv[1] =  -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15]
             - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
    inv[5] =   m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15]
             + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
    inv[9] =  -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15]
             - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
    inv[13] =  m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14]
             + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
    inv[2] =   m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15]
             + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
    inv[6] =  -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15]
             - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
    inv[10] =  m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15]
             + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
    inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14]
             - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
    inv[3] =  -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11]
             - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
    inv[7] =   m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11]
             + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
    inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11]
             - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
    inv[15] =  m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10]
             + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];

    det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
    if (det == 0)
        return GL_FALSE;

    det=1.0f/det;

    for (i = 0; i < 16; i++)
        invOut[i] = inv[i] * det;

    return GL_TRUE;
}

static void __gluMultMatrixVecf(const GLfloat matrix[16], const GLfloat in[4],
                                GLfloat out[4])
{
    int i;

    for (i=0; i<4; i++)
    {
        out[i] = in[0] * matrix[0*4+i] +
                 in[1] * matrix[1*4+i] +
                 in[2] * matrix[2*4+i] +
                 in[3] * matrix[3*4+i];
    }
}

static void __gluMultMatricesf(const GLfloat a[16], const GLfloat b[16],
                               GLfloat r[16])
{
    int i, j;

    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 4; j++)
        {
            r[i*4+j] = a[i*4+0]*b[0*4+j] +
                       a[i*4+1]*b[1*4+j] +
                       a[i*4+2]*b[2*4+j] +
                       a[i*4+3]*b[3*4+j];
        }
    }
}

struct ContextoInterno {
  // Mapeia um ID para a cor RGB em 22 bits (os dois mais significativos sao para a pilha).
  std::unordered_map<unsigned int, unsigned int> ids;
  unsigned int proximo_id = 0;
  // O bit da pilha em dois bits (valor de 0 a 3).
  unsigned int bit_pilha = 0;
  modo_renderizacao_e modo_renderizacao = MR_RENDER;
  bool depurar_selecao_por_cor = false;  // Mudar para true para depurar selecao por cor.
  GLuint* buffer_selecao = nullptr;
  GLuint tam_buffer = 0;

  inline bool UsarSelecaoPorCor() const {
    return depurar_selecao_por_cor || modo_renderizacao == MR_SELECT;
  }
};

ContextoInterno* g_contexto = nullptr;

// Gera um proximo ID.
void MapeiaId(unsigned int id, GLubyte rgb[3]) {
  unsigned int id_mapeado = g_contexto->proximo_id | (g_contexto->bit_pilha << 22);
  g_contexto->ids.insert(std::make_pair(id_mapeado, id));
  if (g_contexto->proximo_id == ((1 << 22) - 1)) {
    LOG(ERROR) << "Limite de ids alcancado";
  } else {
    ++g_contexto->proximo_id;
  }
  rgb[0] = (id_mapeado & 0xFF);
  rgb[1] = ((id_mapeado >> 8) & 0xFF);
  rgb[2] = ((id_mapeado >> 16) & 0xFF);
}

// Retorna os vertices e normais da face sul do cilindro. Os elementos serao anexados ao fim dos vetores.
#if NOVO_DESENHO == 1
void VerticesNormaisIndicesFaceSulCilindro(
    GLfloat raio_base, GLfloat raio_topo, GLfloat altura, GLint fatias, GLint tocos,
    std::vector<float>* coordenadas, std::vector<float>* normais, std::vector<unsigned short>* indices) {
  float angulo_rotacao_graus = 360.0f / fatias;
  float angulo_rotacao_graus_2 = angulo_rotacao_graus / 2.0f;
  float seno_angulo_rotacao_2 = sinf(angulo_rotacao_graus_2 * GRAUS_PARA_RAD);
  GLfloat tam_lado_base_2 = raio_base * seno_angulo_rotacao_2;
  GLfloat tam_lado_topo_2 = raio_topo * seno_angulo_rotacao_2;
  float cos_angulo_rotacao_2 = cosf(angulo_rotacao_graus_2 * GRAUS_PARA_RAD);
  GLfloat tam_y_base = raio_base * cos_angulo_rotacao_2;
  GLfloat tam_y_topo = raio_topo * cos_angulo_rotacao_2;
  GLfloat vetor_x[3] = { 1.0f, 0.0f, 0.0f };
  GLfloat vetor_cima[3] = { 0.0f, raio_base - raio_topo, altura };
  GLfloat vetor_normal[3];
  // Gera a normal FLAT.
  ProdutoVetorial(vetor_x, vetor_cima, vetor_normal);
  Normaliza(vetor_normal);
  // Gera a normal SMOOTH.
  GLfloat matriz_rotacao[16];
  GLfloat vetor_normal_oeste[3] = { vetor_normal[0], vetor_normal[1], vetor_normal[2] };
  MatrizRotacaoZ(-angulo_rotacao_graus_2, matriz_rotacao);
  MultiplicaMatrizVetor(matriz_rotacao, vetor_normal_oeste);
  GLfloat vetor_normal_leste[3] = { vetor_normal[0], vetor_normal[1], vetor_normal[2] };
  MatrizRotacaoZ(angulo_rotacao_graus_2, matriz_rotacao);
  MultiplicaMatrizVetor(matriz_rotacao, vetor_normal_leste);

  {
    int in = normais->size();
    normais->resize(in + 12);
    float* n = &(*normais)[in];
    n[0] = vetor_normal_oeste[0]; n[1]  = vetor_normal_oeste[1]; n[2] =  vetor_normal_oeste[2];
    n[3] = vetor_normal_leste[0]; n[4]  = vetor_normal_leste[1]; n[5] =  vetor_normal_leste[2];
    n[6] = vetor_normal_leste[0]; n[7]  = vetor_normal_leste[1]; n[8] =  vetor_normal_leste[2];
    n[9] = vetor_normal_oeste[0]; n[10] = vetor_normal_oeste[1]; n[11] = vetor_normal_oeste[2];
  }

  int ic = coordenadas->size();
  {
    int ic = coordenadas->size();
    coordenadas->resize(ic + 12);
    float* c = &(*coordenadas)[ic];
    c[0] = -tam_lado_base_2; c[1]  = -tam_y_base; c[2] = 0.0f;
    c[3] = tam_lado_base_2;  c[4]  = -tam_y_base; c[5] = 0.0f;
    c[6] = tam_lado_topo_2;  c[7]  = -tam_y_topo; c[8] = altura;
    c[9] = -tam_lado_topo_2; c[10] = -tam_y_topo; c[11] = altura;
  }

  {
    // 2 triangulos na face.
    int primeiro_indice = indices->size();
    indices->resize(primeiro_indice + 6);
    unsigned short* ind = &(*indices)[primeiro_indice];
    int ii = ic / 3;
    ind[0] = ii + 0;
    ind[1] = ii + 1;
    ind[2] = ii + 2;
    ind[3] = ii + 0;
    ind[4] = ii + 2;
    ind[5] = ii + 3;
  }
}
#elif NOVO_DESENHO == 2
// Retorna os vertices e normais da face sul do cilindro. A face possui dois triangulos em forma de quadrado, ou seja
// 4 vertices (12 coordenadas). Indices devera ter 6 elementos.
void VerticesNormaisIndicesFaceSulCilindro(
    GLfloat raio_base, GLfloat raio_topo, GLfloat altura, GLint fatias, GLint tocos,
    float* vertices, float* normais, unsigned short* indices) {
  float angulo_rotacao_graus = 360.0f / fatias;
  float angulo_rotacao_graus_2 = angulo_rotacao_graus / 2.0f;
  float seno_angulo_rotacao_2 = sinf(angulo_rotacao_graus_2 * GRAUS_PARA_RAD);
  GLfloat tam_lado_base_2 = raio_base * seno_angulo_rotacao_2;
  GLfloat tam_lado_topo_2 = raio_topo * seno_angulo_rotacao_2;
  float cos_angulo_rotacao_2 = cosf(angulo_rotacao_graus_2 * GRAUS_PARA_RAD);
  GLfloat tam_y_base = raio_base * cos_angulo_rotacao_2;
  GLfloat tam_y_topo = raio_topo * cos_angulo_rotacao_2;
  GLfloat vetor_x[3] = { 1.0f, 0.0f, 0.0f };
  GLfloat vetor_cima[3] = { 0.0f, raio_base - raio_topo, altura };
  GLfloat vetor_normal[3];
  // Gera a normal FLAT.
  ProdutoVetorial(vetor_x, vetor_cima, vetor_normal);
  Normaliza(vetor_normal);
  // Gera a normal SMOOTH.
  GLfloat matriz_rotacao[16];
  GLfloat vetor_normal_oeste[3] = { vetor_normal[0], vetor_normal[1], vetor_normal[2] };
  MatrizRotacaoZ(-angulo_rotacao_graus_2, matriz_rotacao);
  MultiplicaMatrizVetor(matriz_rotacao, vetor_normal_oeste);
  GLfloat vetor_normal_leste[3] = { vetor_normal[0], vetor_normal[1], vetor_normal[2] };
  MatrizRotacaoZ(angulo_rotacao_graus_2, matriz_rotacao);
  MultiplicaMatrizVetor(matriz_rotacao, vetor_normal_leste);

  normais[0] = vetor_normal_oeste[0]; normais[1]  = vetor_normal_oeste[1]; normais[2]  = vetor_normal_oeste[2];
  normais[3] = vetor_normal_leste[0]; normais[4]  = vetor_normal_leste[1]; normais[5]  = vetor_normal_leste[2];
  normais[6] = vetor_normal_leste[0]; normais[7]  = vetor_normal_leste[1]; normais[8]  = vetor_normal_leste[2];
  normais[9] = vetor_normal_oeste[0]; normais[10] = vetor_normal_oeste[1]; normais[11] = vetor_normal_oeste[2];

  vertices[0] = -tam_lado_base_2; vertices[1] = -tam_y_base;  vertices[2] = 0.0f;
  vertices[3] = tam_lado_base_2;  vertices[4] = -tam_y_base;  vertices[5] = 0.0f;
  vertices[6] = tam_lado_topo_2;  vertices[7] = -tam_y_topo;  vertices[8] = altura;
  vertices[9] = -tam_lado_topo_2; vertices[10] = -tam_y_topo; vertices[11] = altura;

  // 2 triangulos na face.
  indices[0] = 0;
  indices[1] = 1;
  indices[2] = 2;
  indices[3] = 0;
  indices[4] = 2;
  indices[5] = 3;
}
#endif

#if NOVO_DESENHO == 1
// Retorna os vertices, normais e indice de vertices do cilindro todo. Ao todo, vertices e normais devera conter 12 * fatias coordenadas,
// enquanto indice devera ter 6 * fatias. Os elementos serao anexados ao fim de cada vetor.
void VerticesNormaisIndicesCilindro(
    GLfloat raio_base, GLfloat raio_topo, GLfloat altura, GLint num_fatias, GLint num_tocos,
    std::vector<float>* coordenadas, std::vector<float>* normais, std::vector<unsigned short>* indices) {
  float angulo_rotacao_graus = 360.0f / num_fatias;
  float angulo_rotacao_rad = angulo_rotacao_graus * GRAUS_PARA_RAD;
  int c_antes = coordenadas->size();
  int i_antes = indices->size();
  VerticesNormaisIndicesFaceSulCilindro(
      raio_base, raio_topo, altura, num_fatias, num_tocos, coordenadas, normais, indices);
  int c_depois = coordenadas->size();
  int c_delta = c_depois - c_antes;
  int c_tam_final = c_antes + (c_delta * num_fatias);
  coordenadas->resize(c_tam_final);
  normais->resize(c_tam_final);
  int i_depois = indices->size();
  int i_delta = i_depois - i_antes;
  int i_tam_final = i_antes + (i_delta * num_fatias);
  indices->resize(i_tam_final);

  float angulo_corrente = angulo_rotacao_rad;
  const unsigned short* iref = &(*indices)[i_antes];
  float* cref_esc = &(*coordenadas)[c_depois];
  float* nref_esc = &(*normais)[c_depois];
  unsigned short* iref_esc = &(*indices)[i_depois];

  for (int i = 1; i < num_fatias; ++i) {
    float cosseno = cosf(angulo_corrente);
    float seno = sinf(angulo_corrente);
    angulo_corrente += angulo_rotacao_rad;
    const float* cref = &(*coordenadas)[c_antes];
    const float* nref = &(*normais)[c_antes];
    // Para cada um dos 4 vertices, roda em Z.
    for (int v = 0; v < 4; ++v) {
      float x0 = cref[0];
      float y0 = cref[1];
      float z0 = cref[2];
      cref += 3;
      cref_esc[0] = x0 * cosseno - y0 * seno;
      cref_esc[1] = x0 * seno + y0 * cosseno;
      cref_esc[2] = z0;
      cref_esc += 3;
      float xn0 = nref[0];
      float yn0 = nref[1];
      float zn0 = nref[2];
      nref += 3;
      nref_esc[0] = xn0 * cosseno - yn0 * seno;
      nref_esc[1] = xn0 * seno + yn0 * cosseno;
      nref_esc[2] = zn0;
      nref_esc += 3;
    }
    // Indices apontam para os vertices. Sao 4 vertices por fatia (com
    // replicacao de 2), portanto, sao 6 indices.
    for (int j = 0; j < 6; ++j) {
      iref_esc[j] = iref[j] + 4;
    }
    iref += 6;
    iref_esc += 6;
  }
}
#elif NOVO_DESENHO == 2
// Retorna os vertices, normais e indice de vertices do cilindro todo. Ao todo, vertices e normais devera conter 12 * fatias coordenadas,
// enquanto indice devera ter 6 * fatias.
void VerticesNormaisIndicesCilindro(
    GLfloat raio_base, GLfloat raio_topo, GLfloat altura, GLint fatias, GLint tocos,
    float* vertices, float* normais, unsigned short* indices) {
  float angulo_rotacao_graus = 360.0f / fatias;
  float angulo_rotacao_rad = angulo_rotacao_graus * GRAUS_PARA_RAD;
  VerticesNormaisIndicesFaceSulCilindro(raio_base, raio_topo, altura, fatias, tocos, vertices, normais, indices);

  int indice_destino = 12;  // onde escrever os vertices.
  float angulo_corrente = angulo_rotacao_rad;
  for (int i = 1; i < fatias; ++i) {
    int indice_origem = 0;
    float cosseno = cosf(angulo_corrente);
    float seno = sinf(angulo_corrente);
    angulo_corrente += angulo_rotacao_rad;
    // Para cada um dos 4 vertices, roda em Z.
    for (int v = 0; v < 4; ++v) {
      float x0 = vertices[indice_origem];
      float y0 = vertices[indice_origem + 1];
      float z0 = vertices[indice_origem + 2];
      vertices[indice_destino]     = x0 * cosseno - y0 * seno;
      vertices[indice_destino + 1] = x0 * seno + y0 * cosseno;
      vertices[indice_destino + 2] = z0;
      float xn0 = normais[indice_origem];
      float yn0 = normais[indice_origem + 1];
      float zn0 = normais[indice_origem + 2];
      normais[indice_destino]     = xn0 * cosseno - yn0 * seno;
      normais[indice_destino + 1] = xn0 * seno + yn0 * cosseno;
      normais[indice_destino + 2] = zn0;
      indice_origem += 3;
      indice_destino += 3;
    }
    // Indices apontam para os vertices. Sao 4 vertices por fatia (com
    // replicacao de 2), portanto, sao 6 indices.
    int indice_indices = 6 * i;
    int indice_vertice_inicial = 4 * i;
    indices[indice_indices]     = indice_vertice_inicial;
    indices[indice_indices + 1] = indice_vertice_inicial + 1;
    indices[indice_indices + 2] = indice_vertice_inicial + 2;
    indices[indice_indices + 3] = indice_vertice_inicial + 0;
    indices[indice_indices + 4] = indice_vertice_inicial + 2;
    indices[indice_indices + 5] = indice_vertice_inicial + 3;
  }
}

#endif

// Cubo de tamanho 1.
void CuboSolidoUnitario() {
  unsigned short indices[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
  const float vertices_sul[] = {
    -0.5f, -0.5f, -0.5f,
    0.5f, -0.5f, -0.5f,
    0.5f, -0.5f, 0.5f,
    -0.5f, -0.5f, 0.5f
  };
  const float vertices_norte[] = {
    -0.5f, 0.5f, -0.5f,
    -0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, -0.5f,
  };
  const float vertices_oeste[] = {
    -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f, 0.5f,
    -0.5f, 0.5f, 0.5f,
    -0.5f, 0.5f, -0.5f,
  };
  const float vertices_leste[] = {
    0.5f, -0.5f, -0.5f,
    0.5f, 0.5f, -0.5f,
    0.5f, 0.5f, 0.5f,
    0.5f, -0.5f, 0.5f,
  };
  const float vertices_cima[] = {
    -0.5f, -0.5f, 0.5f,
    0.5f, -0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,
    -0.5f, 0.5f, 0.5f,
  };
  const float vertices_baixo[] = {
    -0.5f, -0.5f, -0.5f,
    -0.5f, 0.5f, -0.5f,
    0.5f, 0.5f, -0.5f,
    0.5f, -0.5f, -0.5f,
  };

  // TODO fazer tudo num desenho so.
  gl::HabilitaEstadoCliente(GL_VERTEX_ARRAY);
  glNormal3f(0.0f, -1.0f, 0.0f);
  gl::PonteiroVertices(3, GL_FLOAT, vertices_sul);
  gl::DesenhaElementos(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_SHORT, indices);
  glNormal3f(0.0f, 1.0f, 0.0f);
  gl::PonteiroVertices(3, GL_FLOAT, vertices_norte);
  gl::DesenhaElementos(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_SHORT, indices);
  glNormal3f(-1.0f, 0.0f, 0.0f);
  gl::PonteiroVertices(3, GL_FLOAT, vertices_oeste);
  gl::DesenhaElementos(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_SHORT, indices);
  glNormal3f(1.0f, 0.0f, 0.0f);
  gl::PonteiroVertices(3, GL_FLOAT, vertices_leste);
  gl::DesenhaElementos(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_SHORT, indices);
  glNormal3f(0.0f, 0.0f, 1.0f);
  gl::PonteiroVertices(3, GL_FLOAT, vertices_cima);
  gl::DesenhaElementos(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_SHORT, indices);
  glNormal3f(0.0f, 0.0f, -1.0f);
  gl::PonteiroVertices(3, GL_FLOAT, vertices_baixo);
  gl::DesenhaElementos(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_SHORT, indices);
  gl::DesabilitaEstadoCliente(GL_VERTEX_ARRAY);
}

}  // namespace

void IniciaGl(int* argcp, char** argv) {
  g_contexto = new ContextoInterno;
}

void FinalizaGl() {
  delete g_contexto;
}

void InicioCena() {
  if (g_contexto->UsarSelecaoPorCor()) {
    g_contexto->proximo_id = 0;
    g_contexto->bit_pilha = 0;
    g_contexto->ids.clear();
  }
}

void Habilita(GLenum cap) {
  if (g_contexto->UsarSelecaoPorCor() && (cap & GL_LIGHTING) != 0) {
    // Sem luz no modo de selecao.
    glDisable(cap);
  } else {
    glEnable(cap);
  }
}

void Retangulo(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2) {
  const unsigned short indices[] = { 0, 1, 2, 3, 4, 5, 6, 7, };
  const float vertices[] = {
    x1, y1,
    x2, y1,
    x2, y2,
    x1, y2,
  };
  gl::HabilitaEstadoCliente(GL_VERTEX_ARRAY);
  gl::PonteiroVertices(2, GL_FLOAT, vertices);
  gl::DesenhaElementos(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_SHORT, indices);
  gl::DesabilitaEstadoCliente(GL_VERTEX_ARRAY);
}

void ConeSolido(GLfloat base, GLfloat altura, GLint num_fatias, GLint num_tocos) {
  CilindroSolido(base, 0.0f, altura, num_fatias, num_tocos);
}

void EsferaSolida(GLfloat raio, GLint num_fatias, GLint num_tocos) {
#if NOVO_DESENHO == 1
  // Desenhar a esfera baseada em cilindros.
  float angulo_rad = (90.0f * GRAUS_PARA_RAD) / num_tocos;
  GLfloat raio_base = raio;
  GLfloat raio_topo;

  // Vertices.
  const int num_vertices_por_fatia = 4;
  const int num_vertices_por_toco = num_vertices_por_fatia * num_fatias;
  const int num_coordenadas_por_toco = num_vertices_por_toco * 3;
  const int num_indices_por_fatia = 6;
  const int num_indices_por_toco = num_indices_por_fatia * num_fatias;

  std::vector<float> coordenadas;
  coordenadas.reserve(num_coordenadas_por_toco * num_tocos * 2);
  std::vector<float> normais;
  normais.reserve(num_coordenadas_por_toco * num_tocos * 2);
  std::vector<unsigned short> indices;
  indices.reserve(num_indices_por_toco * num_tocos * 2);

  float* p_coord = &coordenadas[0];
  for (int i = 0; i < num_tocos; ++i) {
    raio_topo = raio * cosf(angulo_rad * (i + 1));
    GLfloat h_base = raio * sinf(angulo_rad * i);
    GLfloat h_topo = raio * sinf(angulo_rad * (i + 1));
    GLfloat h_delta = h_topo - h_base;
    // Desenha cilindro de cima e de baixo.
    {
      VerticesNormaisIndicesCilindro(raio_base, raio_topo, h_delta, num_fatias, 1,
                                     &coordenadas, &normais, &indices);
      // Translada os Z dos vertices.
      for (int c = 2; c < num_coordenadas_por_toco; c += 3) {
        p_coord[c] += h_base;
      }
      p_coord += num_coordenadas_por_toco;
    }
    {
      // TODO usar simetria.
      VerticesNormaisIndicesCilindro(raio_topo, raio_base, h_delta, num_fatias, 1,
                                     &coordenadas, &normais, &indices);
      // Translada os Z dos vertices.
      for (int c = 2; c < num_coordenadas_por_toco; c += 3) {
        p_coord[c] -= h_topo;
      }
      p_coord += num_coordenadas_por_toco;
    }
    raio_base = raio_topo;
  }
  HabilitaEstadoCliente(GL_VERTEX_ARRAY);
  HabilitaEstadoCliente(GL_NORMAL_ARRAY);
  PonteiroNormais(GL_FLOAT, &normais[0]);
  PonteiroVertices(3, GL_FLOAT, &coordenadas[0]);
  DesenhaElementos(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, &indices[0]);
  DesabilitaEstadoCliente(GL_NORMAL_ARRAY);
  DesabilitaEstadoCliente(GL_VERTEX_ARRAY);
#elif NOVO_DESENHO == 2
  // Desenhar a esfera baseada em cilindros.
  float angulo_rad = (90.0f * GRAUS_PARA_RAD) / num_tocos;
  GLfloat raio_base = raio;
  GLfloat raio_topo;

  // Vertices.
  const int num_vertices_por_fatia = 4;
  const int num_vertices_por_toco = num_vertices_por_fatia * num_fatias;
  const int num_coordenadas_por_toco = num_vertices_por_toco * 3;
  const int num_coordenadas = num_coordenadas_por_toco * num_tocos * 2;
  // Indices.
  const int num_indices_por_fatia = 6;
  const int num_indices_por_toco = num_indices_por_fatia * num_fatias;
  const int num_indices = num_indices_por_toco * num_tocos * 2;

  float coordenadas[num_coordenadas];
  float normais[num_coordenadas];
  unsigned short indices[num_indices];
  int p_coordenadas = 0;
  int p_normais = 0;
  int p_indices = 0;

  for (int i = 0; i < num_tocos; ++i) {
    raio_topo = raio * cosf(angulo_rad * (i + 1));
    GLfloat h_base = raio * sinf(angulo_rad * i);
    GLfloat h_topo = raio * sinf(angulo_rad * (i + 1));
    GLfloat h_delta = h_topo - h_base;
    // Desenha cilindro de cima e de baixo.
    {
      VerticesNormaisIndicesCilindro(raio_base, raio_topo, h_delta, num_fatias, 1,
                                     &coordenadas[p_coordenadas], &normais[p_normais], &indices[p_indices]);
      // Translada os Z dos vertices.
      for (int c = 2; c < num_coordenadas_por_toco; c += 3) {
        coordenadas[p_coordenadas + c] += h_base;
      }
      int inicio_vertices = p_coordenadas / 3;
      for (int ii = 0; ii < num_indices_por_toco; ++ii) {
        indices[p_indices + ii] += inicio_vertices;
      }
      p_coordenadas += num_coordenadas_por_toco;
      p_normais += num_coordenadas_por_toco;
      p_indices += num_indices_por_toco;
    }
    {
      // TODO usar simetria.
      VerticesNormaisIndicesCilindro(raio_topo, raio_base, h_delta, num_fatias, 1,
                                     &coordenadas[p_coordenadas], &normais[p_normais], &indices[p_indices]);
      // Translada os Z dos vertices.
      for (int c = 2; c < num_coordenadas_por_toco; c += 3) {
        coordenadas[p_coordenadas + c] -= h_topo;
      }
      int inicio_vertices = p_coordenadas / 3;
      for (int ii = 0; ii < num_indices_por_toco; ++ii) {
        indices[p_indices + ii] += inicio_vertices;
      }
      p_coordenadas += num_coordenadas_por_toco;
      p_normais += num_coordenadas_por_toco;
      p_indices += num_indices_por_toco;
    }
    raio_base = raio_topo;
  }
  HabilitaEstadoCliente(GL_VERTEX_ARRAY);
  HabilitaEstadoCliente(GL_NORMAL_ARRAY);
  PonteiroNormais(GL_FLOAT, normais);
  PonteiroVertices(3, GL_FLOAT, coordenadas);
  DesenhaElementos(GL_TRIANGLES, num_indices, GL_UNSIGNED_SHORT, indices);
  DesabilitaEstadoCliente(GL_NORMAL_ARRAY);
  DesabilitaEstadoCliente(GL_VERTEX_ARRAY);
#else
  // Desenhar a esfera baseada em cilindros.
  float angulo_rad = (90.0f * GRAUS_PARA_RAD) / num_tocos;
  GLfloat raio_base = raio;
  GLfloat raio_topo;

  for (int i = 0; i < num_tocos; ++i) {
    raio_topo = raio * cosf(angulo_rad * (i + 1));
    GLfloat h_base = raio * sinf(angulo_rad * i);
    GLfloat h_topo = raio * sinf(angulo_rad * (i + 1));
    GLfloat h_delta = h_topo - h_base;
    // Desenha cilindro de cima e de baixo.
    {
      MatrizEscopo salva_matriz;
      glTranslatef(0.0f, 0.0f, h_base);
      CilindroSolido(raio_base, raio_topo, h_delta, num_fatias, 1);
    }
    {
      MatrizEscopo salva_matriz;
      glTranslatef(0.0f, 0.0f, -h_topo);
      CilindroSolido(raio_topo, raio_base, h_delta, num_fatias, 1);
    }
    raio_base = raio_topo;
  }
#endif
}

void CuboSolido(GLfloat tam_lado) {
  gl::MatrizEscopo salva_matriz;
  gl::Escala(tam_lado, tam_lado, tam_lado);
  CuboSolidoUnitario();
}

void CilindroSolido(GLfloat raio_base, GLfloat raio_topo, GLfloat altura, GLint num_fatias, GLint num_tocos) {
#if NOVO_DESENHO == 1
  const int num_vertices_por_fatia = 4;
  const int num_vertices_por_toco = num_vertices_por_fatia * num_fatias;
  const int num_coordenadas_por_toco = num_vertices_por_toco * 3;
  const int num_indices_por_fatia = 6;
  const int num_indices_por_toco = num_indices_por_fatia * num_fatias;

  std::vector<float> coordenadas;
  coordenadas.reserve(num_coordenadas_por_toco * num_tocos);
  std::vector<float> normais;
  normais.reserve(num_coordenadas_por_toco * num_tocos);
  std::vector<unsigned short> indices;
  indices.reserve(num_indices_por_toco * num_tocos);
  VerticesNormaisIndicesCilindro(raio_base, raio_topo, altura, num_fatias, num_tocos, &coordenadas, &normais, &indices);

  HabilitaEstadoCliente(GL_VERTEX_ARRAY);
  HabilitaEstadoCliente(GL_NORMAL_ARRAY);
  PonteiroNormais(GL_FLOAT, &normais[0]);
  PonteiroVertices(3, GL_FLOAT, &coordenadas[0]);
  DesenhaElementos(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, &indices[0]);
  DesabilitaEstadoCliente(GL_NORMAL_ARRAY);
  DesabilitaEstadoCliente(GL_VERTEX_ARRAY);
#elif NOVO_DESENHO == 2
  const int num_vertices_por_fatia = 4;  // os 4 vertices formam 2 triangulos (ha repeticao).
  const int num_vertices = num_vertices_por_fatia * num_fatias;
  const int num_coordenadas = 3 * num_vertices;
  const int num_indices_por_fatia = 6;
  const int num_indices = num_indices_por_fatia * num_fatias;
  float vertices[num_coordenadas];
  float normais[num_coordenadas];
  unsigned short indices[num_indices];
  VerticesNormaisIndicesCilindro(raio_base, raio_topo, altura, num_fatias, num_tocos, vertices, normais, indices);

  HabilitaEstadoCliente(GL_VERTEX_ARRAY);
  HabilitaEstadoCliente(GL_NORMAL_ARRAY);
  PonteiroNormais(GL_FLOAT, normais);
  PonteiroVertices(3, GL_FLOAT, vertices);
  DesenhaElementos(GL_TRIANGLES, num_indices, GL_UNSIGNED_SHORT, indices);
  DesabilitaEstadoCliente(GL_NORMAL_ARRAY);
  DesabilitaEstadoCliente(GL_VERTEX_ARRAY);
#else
  // Versao antiga.
  gl::MatrizEscopo salva_matriz;
  float angulo_rotacao_graus = 360.0f / num_fatias;
  float angulo_rotacao_graus_2 = angulo_rotacao_graus / 2.0f;
  unsigned short indices[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
  float seno_angulo_rotacao_2 = sinf(angulo_rotacao_graus_2 * GRAUS_PARA_RAD);
  GLfloat tam_lado_base_2 = raio_base * seno_angulo_rotacao_2;
  GLfloat tam_lado_topo_2 = raio_topo * seno_angulo_rotacao_2;
  float cos_angulo_rotacao_2 = cosf(angulo_rotacao_graus_2 * GRAUS_PARA_RAD);
  GLfloat tam_y_base = raio_base * cos_angulo_rotacao_2;
  GLfloat tam_y_topo = raio_topo * cos_angulo_rotacao_2;
  GLfloat vetor_x[3] = { 1.0f, 0.0f, 0.0f };
  GLfloat vetor_cima[3] = { 0.0f, raio_base - raio_topo, altura };
  GLfloat vetor_normal[3];
  // Gera a normal FLAT.
  ProdutoVetorial(vetor_x, vetor_cima, vetor_normal);
  Normaliza(vetor_normal);
  // Gera a normal SMOOTH.
  GLfloat matriz_rotacao[16];
  GLfloat vetor_normal_oeste[3] = { vetor_normal[0], vetor_normal[1], vetor_normal[2] };
  MatrizRotacaoZ(-angulo_rotacao_graus_2, matriz_rotacao);
  MultiplicaMatrizVetor(matriz_rotacao, vetor_normal_oeste);
  GLfloat vetor_normal_leste[3] = { vetor_normal[0], vetor_normal[1], vetor_normal[2] };
  MatrizRotacaoZ(angulo_rotacao_graus_2, matriz_rotacao);
  MultiplicaMatrizVetor(matriz_rotacao, vetor_normal_leste);
  const float vertices_normais[12] = {
    vetor_normal_oeste[0], vetor_normal_oeste[1], vetor_normal_oeste[2],
    vetor_normal_leste[0], vetor_normal_leste[1], vetor_normal_leste[2],
    vetor_normal_leste[0], vetor_normal_leste[1], vetor_normal_leste[2],
    vetor_normal_oeste[0], vetor_normal_oeste[1], vetor_normal_oeste[2],
  };
  const float vertices_sul[] = {
    -tam_lado_base_2, -tam_y_base, 0.0f,
    tam_lado_base_2, -tam_y_base, 0.0f,
    tam_lado_topo_2, -tam_y_topo, altura,
    -tam_lado_topo_2, -tam_y_topo, altura
  };

  HabilitaEstadoCliente(GL_VERTEX_ARRAY);
  HabilitaEstadoCliente(GL_NORMAL_ARRAY);
  for (int i = 0; i < num_fatias; ++i) {
    // TODO pra que esse normal aqui se tem o PonteiroNormais?
    Normal(vetor_normal[0], vetor_normal[1], vetor_normal[2]);
    PonteiroNormais(GL_FLOAT, vertices_normais);
    PonteiroVertices(3, GL_FLOAT, vertices_sul);
    DesenhaElementos(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_SHORT, indices);
    Roda(angulo_rotacao_graus, 0.0f, 0.0f, 1.0f);
  }
  DesabilitaEstadoCliente(GL_NORMAL_ARRAY);
  DesabilitaEstadoCliente(GL_VERTEX_ARRAY);
#endif
}

void Perspectiva(GLfloat fovy, GLfloat aspect, GLfloat zNear, GLfloat zFar) {
  // Copiado do glues.
  GLfloat m[4][4];
  GLfloat sine, cotangent, deltaZ;
  GLfloat radians=(GLfloat)(fovy/2.0f*__glPi/180.0f);

  deltaZ=zFar-zNear;
  sine=(GLfloat)sinf(radians);
  if ((deltaZ==0.0f) || (sine==0.0f) || (aspect==0.0f))
  {
      return;
  }
  cotangent=(GLfloat)(cos(radians)/sine);

  PreencheIdentidade(&m[0][0]);
  m[0][0] = cotangent / aspect;
  m[1][1] = cotangent;
  m[2][2] = -(zFar + zNear) / deltaZ;
  m[2][3] = -1.0f;
  m[3][2] = -2.0f * zNear * zFar / deltaZ;
  m[3][3] = 0;
  glMultMatrixf(&m[0][0]);
}

void OlharPara(GLfloat eyex, GLfloat eyey, GLfloat eyez, GLfloat centerx,
               GLfloat centery, GLfloat centerz,
               GLfloat upx, GLfloat upy, GLfloat upz) {
  GLfloat forward[3], side[3], up[3];
  GLfloat m[4][4];

  forward[0] = centerx - eyex;
  forward[1] = centery - eyey;
  forward[2] = centerz - eyez;

  up[0] = upx;
  up[1] = upy;
  up[2] = upz;

  Normaliza(forward);

  /* Side = forward x up */
  ProdutoVetorial(forward, up, side);
  Normaliza(side);

  /* Recompute up as: up = side x forward */
  ProdutoVetorial(side, forward, up);

  PreencheIdentidade(&m[0][0]);
  m[0][0] = side[0];
  m[1][0] = side[1];
  m[2][0] = side[2];

  m[0][1] = up[0];
  m[1][1] = up[1];
  m[2][1] = up[2];

  m[0][2] = -forward[0];
  m[1][2] = -forward[1];
  m[2][2] = -forward[2];

  glMultMatrixf(&m[0][0]);
  glTranslatef(-eyex, -eyey, -eyez);
}

GLint Desprojeta(GLfloat winx, GLfloat winy, GLfloat winz,
                 const GLfloat modelMatrix[16],
                 const GLfloat projMatrix[16],
                 const GLint viewport[4],
                 GLfloat* objx, GLfloat* objy, GLfloat* objz) {
  GLfloat finalMatrix[16];
  GLfloat in[4];
  GLfloat out[4];

  __gluMultMatricesf(modelMatrix, projMatrix, finalMatrix);
  if (!__gluInvertMatrixf(finalMatrix, finalMatrix))
  {
      return(GL_FALSE);
  }

  in[0]=winx;
  in[1]=winy;
  in[2]=winz;
  in[3]=1.0;

  /* Map x and y from window coordinates */
  in[0] = (in[0] - viewport[0]) / viewport[2];
  in[1] = (in[1] - viewport[1]) / viewport[3];

  /* Map to range -1 to 1 */
  in[0] = in[0] * 2 - 1;
  in[1] = in[1] * 2 - 1;
  in[2] = in[2] * 2 - 1;

  __gluMultMatrixVecf(finalMatrix, in, out);
  if (out[3] == 0.0)
  {
      return(GL_FALSE);
  }

  out[0] /= out[3];
  out[1] /= out[3];
  out[2] /= out[3];
  *objx = out[0];
  *objy = out[1];
  *objz = out[2];

  return(GL_TRUE);
}

void MatrizPicking(float x, float y, float delta_x, float delta_y, GLint *viewport) {
  if (delta_x <= 0 || delta_y <= 0) {
      return;
  }

  /* Translate and scale the picked region to the entire window */
  glTranslatef((viewport[2] - 2 * (x - viewport[0])) / delta_x,
               (viewport[3] - 2 * (y - viewport[1])) / delta_y, 0);
  glScalef(viewport[2] / delta_x, viewport[3] / delta_y, 1.0);
}

GLint ModoRenderizacao(modo_renderizacao_e modo) {
  if (g_contexto->modo_renderizacao == modo) {
    VLOG(1) << "Nao houve mudanca no modo de renderizacao";
    return 0;
  }
  g_contexto->modo_renderizacao = modo;
  switch (modo) {
    case MR_SELECT:
      return 0;
    case MR_RENDER: {
      glFlush();
      glFinish();
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      GLubyte pixel[4] = { 0 };
      glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
      int erro = glGetError();
      if (erro != 0) {
        VLOG(1) << "Erro pos glReadPixels: " << erro;
      }
      // Usar void* para imprimir em hexa.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
      VLOG(2) << "Pixel: " << (void*)pixel[0] << " " << (void*)pixel[1] << " " << (void*)pixel[2] << " " << (void*)pixel[3];;
      unsigned int id_mapeado = pixel[0] | (pixel[1] << 8) | (pixel[2] << 16);
      VLOG(1) << "Id mapeado: " << (void*)id_mapeado;
      unsigned int pos_pilha = id_mapeado >> 22;
      VLOG(1) << "Pos pilha: " << pos_pilha;
      if (pos_pilha == 0 || pos_pilha > 3) {
        LOG(ERROR) << "Pos pilha invalido: " << pos_pilha;
        return 0;
      }
      auto it = g_contexto->ids.find(id_mapeado);
      if (it == g_contexto->ids.end()) {
        LOG(ERROR) << "Id nao mapeado: " << (void*)id_mapeado;
        return 0;
      }
#pragma GCC diagnostic pop
      unsigned int id_original = it->second;
      VLOG(1) << "Id original: " << id_original;
      GLuint* ptr = g_contexto->buffer_selecao;
      ptr[0] = pos_pilha;
      ptr[1] = 0;  // zmin.
      ptr[2] = 0;  // zmax
      for (unsigned int i = 0; i < pos_pilha; ++i) {
        ptr[3 + i] = id_original;
      }
      g_contexto->buffer_selecao = nullptr;
      g_contexto->tam_buffer = 0;
      return 1;  // Numero de hits: so pode ser 0 ou 1.
    }
    default:
      return 0;
  }
}

void BufferSelecao(GLsizei tam_buffer, GLuint* buffer) {
  g_contexto->buffer_selecao = buffer;
  g_contexto->tam_buffer = tam_buffer;
}

// Nomes
void IniciaNomes() {
}

void EmpilhaNome(GLuint id) {
  if (!g_contexto->UsarSelecaoPorCor()) {
    // So muda no modo de selecao.
    return;
  }
  if (g_contexto->bit_pilha == 3) {
    LOG(ERROR) << "Bit da pilha passou do limite superior.";
    return;
  }
  ++g_contexto->bit_pilha;
}

void CarregaNome(GLuint id) {
  if (!g_contexto->UsarSelecaoPorCor()) {
    // So muda no modo de selecao.
    return;
  }
  GLubyte rgb[3];
  MapeiaId(id, rgb);
  // Muda a cor para a mapeada.
  glColor4ub(rgb[0], rgb[1], rgb[2], 1.0f);
}

void DesempilhaNome() {
  if (!g_contexto->UsarSelecaoPorCor()) {
    // So muda no modo de selecao.
    return;
  }
  if (g_contexto->bit_pilha == 0) {
    LOG(ERROR) << "Bit da pilha passou do limite inferior.";
    return;
  }
  --g_contexto->bit_pilha;
}

void MudaCor(float r, float g, float b, float a) {
  if (g_contexto->UsarSelecaoPorCor()) {
    // So muda no modo de renderizacao pra nao estragar o picking por cor.
    return;
  }
  GLfloat cor[4] = { r, g, b, a };
  // Segundo manual do OpenGL ES, nao se pode definir o material separadamente por face.
  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, cor);
  glColor4f(r, g, b, a);
}

void Limpa(GLbitfield mascara) {
  if (g_contexto->UsarSelecaoPorCor()) {
    if ((mascara & GL_COLOR_BUFFER_BIT) != 0) {
      // Preto nao eh valido no color picking.
      glClearColor(0, 0, 0, 1.0f);
    }
  }
  glClear(mascara);
}

void DesenhaString(const std::string& str) {
  gl::MatrizEscopo salva_matriz;
  // Inverte Y e Z e centraliza.
  gl::Roda(90.0f, 1.0f, 0.0f, 0.0f);
  gl::Escala(0.6f, 1.0f, 1.0f);
  gl::Translada(-str.size() / 2.0f, 0.0f, 0.0f);
  // TODO: colocar na escala certa.
  for (const char c : str) {
    gl::DesenhaCaractere(c);
    gl::Translada(1.0f, 0.0f, 0.0f);
  }
}

void DesenhaCaractere(char c) {
  const auto& caractere_it = g_indices_caracteres.find(c);
  if (caractere_it == g_indices_caracteres.end()) {
    return;
  }
  gl::HabilitaEstadoCliente(GL_VERTEX_ARRAY);
  gl::PonteiroVertices(2, GL_FLOAT, &g_vertices_caracteres[0]);
  gl::DesenhaElementos(GL_TRIANGLES, caractere_it->second.size(), GL_UNSIGNED_SHORT, &caractere_it->second[0]);
  gl::DesabilitaEstadoCliente(GL_VERTEX_ARRAY);
}

}  // namespace gl

#endif
