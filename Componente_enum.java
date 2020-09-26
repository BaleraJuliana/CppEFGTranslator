package leitura_interface;


import java.util.LinkedList;

public class Componente_enum {
	
	private nome_componente tipo;
	private tipo_no tipo_no;
	private String nome_variavel; 
	private LinkedList<Componente_enum> nos_que_saem; 
	
	public Componente_enum(){
		nos_que_saem = new LinkedList<Componente_enum>();
	}

	public void addNo(Componente_enum no) {
		nos_que_saem.add(no);
	}
	
	public LinkedList<Componente_enum> getNos_que_saem() {
		return nos_que_saem;
	}
	
	public tipo_no getTipo_no() {
		return tipo_no;
	}

	public void setTipo_no(tipo_no tipo_no) {
		this.tipo_no = tipo_no;
	}
	
	public nome_componente getTipo() {
		return tipo;
	}

	public void setTipo(nome_componente tipo) {
		this.tipo = tipo;
	}
	
	public String getNome_variavel() {
		return nome_variavel;
	}

	public void setNome_variavel(String nome) {
		nome_variavel = nome;
	}
	
	
}
