package leitura_interface;

//// essa classe lê xml. arquivos com extensão .ui

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.LinkedList;
import java.util.Scanner;
import java.util.regex.Pattern;

public class Interface {

	private Biblioteca biblioteca;
	private LinkedList<Window> minhas_janelas;
	private String caminho_arquivo_interface_ui;
	private String caminho_arquivo_interface_cpp;
	public static Interface uniqueInstance;
	
	
	private Interface() {
		caminho_arquivo_interface_cpp = "";
		caminho_arquivo_interface_ui = "";
		minhas_janelas = new LinkedList<Window>();
		biblioteca = Biblioteca.getInstance();
	}
	   
	public static Interface getInstance() {
		if(uniqueInstance==null) {
			uniqueInstance = new Interface();
		}
		return uniqueInstance;
	}
	
	public void SetCaminhos(String caminho_ui, String caminho_cpp) {
		caminho_arquivo_interface_ui = caminho_ui;
		caminho_arquivo_interface_cpp = caminho_cpp;
	}
	
	public void SetBiblioteca(biblioteca_interface biblioteca) {
		this.biblioteca = this.biblioteca;
	}
	
	public void identificar_janelas(){
		//// cria uma �nica grid
	    	     Window novo = new Window();
	    	     novo.setCaminho_arquivo_interface(caminho_arquivo_interface_ui, caminho_arquivo_interface_cpp);
	    		 novo.setNome(pega_nome_variavel("", nome_componente.grid));
	    		 novo.setLinha(0);
	    		 this.add_janela(novo);
	  }
	

	public void identificar_janelas_ver1(){
		//// identifica todas as grids
	    
	    try {
	     
	      FileReader arq = new FileReader(caminho_arquivo_interface_ui);
	      BufferedReader lerArq = new BufferedReader(arq);
	 
	      String linha = lerArq.readLine(); 
	      int num_linha = 0;
	      
	      while (linha != null) {
	    	  
	    	  num_linha = num_linha+1;
	    	  
	    	  if(linha.contains("//")){
	    		  linha = lerArq.readLine(); // le da segunda ate a ultima linha
	    		  continue;
	    	  }
	    	  
	    	  // converte tudo para letra minuscula
	    	  linha = linha.toLowerCase();
	    	  
	    	  //if(linha.contains("layout") && !(linha.contains("/"))){
	    	  if(linha.contains("<class>")){ 
	    	     Window novo = new Window();
	    	     novo.setCaminho_arquivo_interface(caminho_arquivo_interface_ui, caminho_arquivo_interface_cpp);
	    		 novo.setNome(pega_nome_variavel(linha, nome_componente.grid));
	    		 novo.setLinha(num_linha);
	    		 this.add_janela(novo);
	    		 
	    	 }
	        
	         linha = lerArq.readLine(); // le da segunda ate a ultima linha
	       }
	 
	      arq.close();
	      
	    } catch (IOException e) {
	        System.err.printf("Erro na abertura do arquivo: %s.\n Classe: Interface_qt metodo: identificar_janelas()",
	          e.getMessage());
	        
	    }
	  }
	
	public void lerComponentes() {
		
		this.identificar_janelas();
		for(Window janelas : minhas_janelas) {
			janelas.ler_interface();
			janelas.completar();
			janelas.identificar_ligacoes();
		}
	}
	

	private String pega_nome_variavel(String linha, nome_componente tipo){
		
		String pega_nome = "";
		String[] nome = new String[10];
		for(String palavra : linha.split(" ")){
			if((palavra.contains(("name=")))){
				nome = palavra.split("=");
				pega_nome = nome[1];
				break;
			}
		}
		pega_nome = pega_nome.replace(">", "");
		pega_nome = pega_nome.replace("\"", "");
		
		return pega_nome;
	}
	
	
	private void add_janela(Window window){
		minhas_janelas.add(window);
	}
	
	public LinkedList<Window> getJanelas() {
		return minhas_janelas;
	}
	
	public String getCaminho_arquivo_interface() {
		return caminho_arquivo_interface_ui;
	}

	public void setCaminho_arquivo_interface(String caminho_ui, String caminho_cpp) {
		caminho_arquivo_interface_ui = caminho_ui;
		caminho_arquivo_interface_cpp = caminho_cpp;
	}
	
}
