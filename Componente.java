package leitura_interface;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.LinkedList;

public class Componente {
	
	private nome_componente tipo;
	private tipo_no tipo_no;
	private String funcao_acao;
	private String nome_variavel; 
	private LinkedList<Componente> nos_que_saem; 
	
	public Componente(){
		nos_que_saem = new LinkedList<Componente>();
	}

	

	public void identificar_acao(String caminho_arquivo_interface_cpp) {
			
		    try {
		    	
		      FileReader arq = new FileReader(caminho_arquivo_interface_cpp);
		      BufferedReader lerArq = new BufferedReader(arq);
		 
		      String linha = lerArq.readLine(); 
		      String pega_nome = "";
		      while (linha != null) {
		    	  
		    	  
		    	  linha = linha.toLowerCase();
		    	  String[] palavras = new String[10];
		    	  if(linha.contains("//")) {
		    		  linha = lerArq.readLine();
		    		  continue;
		    	  }
		    	  if(linha.contains("connect") && linha.contains(this.nome_variavel)) {
		    		  
		    		  
		    		  
		    		  palavras = linha.split(",");
		    		  
		    		  
		    		  pega_nome = palavras[3];
		    		  pega_nome = pega_nome.replace("slot", "");
		    	      pega_nome = pega_nome.replace("(", "");
		    	      pega_nome = pega_nome.replace(")", "");
		    	      pega_nome = pega_nome.replace(";", "");
		    	      pega_nome = pega_nome.replace(" ", "");
		    	      
		    		  break;
		    	  }
		    	  
		    	  linha = lerArq.readLine();    
			   
		    	  
		      }	
		      arq.close();
		      funcao_acao = pega_nome;
		    } catch (IOException e) {
		        System.err.printf("Erro na abertura do arquivo: %s.\n Classe: Window_qt metodo: identificar_no_terminal()",
		          e.getMessage());
		    }
	}   
	
	public void addNo(Componente no) {
		nos_que_saem.add(no);
	}
	
	public LinkedList<Componente> getNos_que_saem() {
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

	public String getFuncao_acao() {
		return funcao_acao;
	}

	public void setFuncao_acao(String funcao_acao) {
		this.funcao_acao = funcao_acao;
	}
	
	
}
