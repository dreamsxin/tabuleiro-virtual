#ifndef ENT_ENTIDADE_H
#define ENT_ENTIDADE_H

namespace ent {

	/** gera um identificador unico para a entidade. */
	int geraId();

	/** parametros de desenho dos objetos. */
	class ParametrosDesenho {};

	/** classe base para entidades. Deve implementar as interfaces de desenho e clique.
	* Toda entidade devera possuir um identificador unico. 
	*/
	class Entidade {
	protected:
		explicit Entidade(int id) { id_ = id; }
	public:
		virtual ~Entidade(){}

	public:
		/** @return o identificador da entidade que deve ser unico. */
		int id() const { return id_; }

		/** trata o clique do objeto.
		* @param id armazenado no buffer de selecao.
		*/
		virtual void clique(int id) = 0;

		/** desenha o objeto, recebendo os parametros de desenho. */
		virtual void desenha(const ParametrosDesenho& pd) = 0;

	private:
		int id_;
	};

}

#endif

