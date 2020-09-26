package leitura_interface;


import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.LinkedList;

public class Window{


	private LinkedList<Componente> componentes;
	private String caminho_arquivo_interface_ui;
	private String caminho_arquivo_interface_cpp;
	private String nome;
	private int posicao_linha;

	public Window(){
		componentes = new LinkedList<Componente>();
		posicao_linha = 0;
	}

	public String getNome() {
		return nome;
	}

	public void setNome(String nome) {
		this.nome = nome;
	}

	public void add_componente(Componente componente){
		componentes.add(componente);
	}

	public LinkedList<Componente> getComponentes() {
		return componentes;
	}


	public void setComponentes(LinkedList<Componente> componentes) {
		this.componentes = componentes;
	}



	public void setCaminho_arquivo_interface(String caminho_ui, String caminho_cpp) {
		caminho_arquivo_interface_ui = caminho_ui;
		caminho_arquivo_interface_cpp = caminho_cpp;
	}

	public int getLinha() {
		return posicao_linha;
	}

	public void setLinha(int linha) {
		this.posicao_linha = linha;
	}

	public void ler_interface(){

		try {

			FileReader arq = new FileReader(caminho_arquivo_interface_ui);
			BufferedReader lerArq = new BufferedReader(arq);

			String linha = lerArq.readLine(); 
			//  boolean eh_essa_janela = false;
			// boolean uma_classe = false;


			int num_linha = 0;
			// boolean marcador = false;

			while (linha != null) {

				linha = linha.toLowerCase();

				if(linha.contains(Biblioteca.getInstance().getButton())){
					Componente novo = new Componente();
					String nome = pega_nome_variavel(linha, nome_componente.button);
					if(nome.isEmpty()) {
						linha = lerArq.readLine();
						continue;
					}
					novo.setNome_variavel(nome);
					novo.setTipo(nome_componente.button);
					if(!verificar_existencia(nome))
						this.add_componente(novo);
				}

				if(linha.contains(Biblioteca.getInstance().getRadiobutton())){
					Componente novo = new Componente();
					String nome = pega_nome_variavel(linha, nome_componente.radiobutton);
					if(nome.isEmpty()) {
						linha = lerArq.readLine();
						continue;
					}
					novo.setNome_variavel(nome);
					novo.setTipo(nome_componente.radiobutton);
					if(!verificar_existencia(nome))
						this.add_componente(novo);
				}

				if(linha.contains(Biblioteca.getInstance().getSpinbutton())){
					Componente novo = new Componente();
					String nome = pega_nome_variavel(linha, nome_componente.spinbutton);
					if(nome.isEmpty()) {
						linha = lerArq.readLine();
						continue;
					}
					novo.setNome_variavel(nome);
					novo.setTipo(nome_componente.spinbutton);
					if(!verificar_existencia(nome))
						this.add_componente(novo);
				}


				if(linha.contains(Biblioteca.getInstance().getQLineEdit())){
					
					Componente novo = new Componente();
					Componente novo_1 = new Componente();
					Componente novo_2 = new Componente();

					String nome = pega_nome_variavel(linha, nome_componente.editline);
					if(nome.isEmpty()) {
						linha = lerArq.readLine();
						continue;
					}
					
					novo.setNome_variavel(nome);
					novo.setTipo(nome_componente.editline);

					novo_1.setNome_variavel("r_invalid_"+nome);
					novo_1.setTipo(nome_componente.r_invalid);
					novo_1.setTipo_no(tipo_no.r_value);
					
					novo_2.setNome_variavel("r_valid_"+nome);
					novo_2.setTipo(nome_componente.r_valid);
					novo_2.setTipo_no(tipo_no.r_value);
					
					novo.addNo(novo_1);
					novo.addNo(novo_2);
					novo_1.addNo(novo);
					novo_2.addNo(novo);
					
					
					if(!verificar_existencia(nome)) {
						this.add_componente(novo);
						this.add_componente(novo_1);
						this.add_componente(novo_2);

					}

				}

				if(linha.contains(Biblioteca.getInstance().getQListWidget())){
					Componente novo = new Componente();
					String nome = pega_nome_variavel(linha, nome_componente.listwidget);
					if(nome.isEmpty()) {
						linha = lerArq.readLine();
						continue;
					}
					novo.setNome_variavel(nome);
					novo.setTipo(nome_componente.listwidget);
					if(!verificar_existencia(nome))
						this.add_componente(novo);
				}

				if(linha.contains(Biblioteca.getInstance().getQComboBox())){

					Componente novo = new Componente();
					Componente novo_1 = new Componente();
					Componente novo_2 = new Componente();

					String nome = pega_nome_variavel(linha, nome_componente.editline);
					if(nome.isEmpty()) {
						linha = lerArq.readLine();
						continue;
					}

					novo.setNome_variavel(nome);
					novo.setTipo(nome_componente.combobox);

					novo_1.setNome_variavel("r_invalid_"+nome);
					novo_1.setTipo(nome_componente.r_invalid);
					novo_1.setTipo_no(tipo_no.r_value);
					novo_2.setNome_variavel("r_valid_"+nome);
					novo_2.setTipo(nome_componente.r_valid);
					novo_2.setTipo_no(tipo_no.r_value);
					
					novo.addNo(novo_1);
					novo.addNo(novo_2);
					novo_1.addNo(novo);
					novo_2.addNo(novo);
					
					if(!verificar_existencia(nome)) {
						this.add_componente(novo);
						this.add_componente(novo_1);
						this.add_componente(novo_2);

					}
				}

				if(linha.contains(Biblioteca.getInstance().getQSlider())){
					Componente novo = new Componente();
					String nome = pega_nome_variavel(linha, nome_componente.slider);
					if(nome.isEmpty()) {
						linha = lerArq.readLine();
						continue;
					}
					novo.setNome_variavel(nome);
					novo.setTipo(nome_componente.slider);
					if(!verificar_existencia(nome))
						this.add_componente(novo);
				}

				if(linha.contains(Biblioteca.getInstance().getQToolButton())){
					Componente novo = new Componente();
					String nome = pega_nome_variavel(linha, nome_componente.toolbutton);
					if(nome.isEmpty()) {
						linha = lerArq.readLine();
						continue;
					}
					novo.setNome_variavel(nome);
					novo.setTipo(nome_componente.toolbutton);
					if(!verificar_existencia(nome))
						this.add_componente(novo);
				}

				if(linha.contains(Biblioteca.getInstance().getQCheckBox())){
					Componente novo = new Componente();
					String nome = pega_nome_variavel(linha, nome_componente.checkbox);
					if(nome.isEmpty()) {
						linha = lerArq.readLine();
						continue;
					}
					novo.setNome_variavel(nome);
					novo.setTipo(nome_componente.checkbox);
					if(!verificar_existencia(nome))
						this.add_componente(novo);
				}
				linha = lerArq.readLine();   
			}

			arq.close();

		} catch (IOException e) {
			System.err.printf("Erro na abertura do arquivo: %s.\n Classe: Window_qt metodo: ler_interface() ",
					e.getMessage());
		}
	}




	
	public void completar(){
		int a = componentes.size();
		int quantidade_nos_terminais = 0;
		// descomentar depois
		// chamar a função depois de tudo


		
	
		while(quantidade_nos_terminais<0.1*a){
			 Componente novo = new Componente();
    		 novo.setNome_variavel("completar");
    		// System.out.println("Nome da variavel: "+novo.getNome_variavel()+" Tipo: "+nome_componente.scale);
    		 novo.setTipo(nome_componente.completa);
    		 this.add_componente(novo);
    		 quantidade_nos_terminais = quantidade_nos_terminais + 1;
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
		pega_nome = pega_nome.replace("/", "");
		pega_nome = pega_nome.replace(" ", "");


		return pega_nome;
	}



	public void identificar_ligacoes() {

		int lala;
		//sequencia obrigatoria
		lala = this.identificar_no_terminal();
		int zeze = this.getComponentes().size();


		// descomentar depois
		/**
		if(lala<2){
			for(int i=0;i<(0.5*zeze);i++){
			Componente_qt novo = new Componente_qt();
   		    novo.setNome_variavel("terminal");
   		    // System.out.println("Nome da variavel: "+novo.getNome_variavel()+" Tipo: "+nome_componente.scale);
   		    novo.setTipo(nome_componente.completa);
   		    novo.setTipo_no(tipo_no.terminal);
   		    this.add_componente(novo);
			}
		}
		 **/

		this.identificar_no_medio();
	}


	private void identificar_no_medio(){


		for(Componente componente : componentes) {
			if(componente.getTipo_no()!=tipo_no.terminal && componente.getTipo_no()!=tipo_no.r_value) {
				componente.setTipo_no(tipo_no.medio);
			}

		} 

	}

	private int identificar_no_terminal(){

		int x = 0;
		for(Componente componente : componentes) {
			int abre_colchetes=0;
			int fecha_colchetes=0;
			boolean to_na_funcao = false;

			componente.identificar_acao(caminho_arquivo_interface_cpp);

			if(componente.getNome_variavel().contains("apply") || componente.getNome_variavel().contains("ok") || componente.getNome_variavel().contains("cancel") || componente.getNome_variavel().contains("close")) {
				componente.setTipo_no(tipo_no.terminal);
				continue;
			}

			if(componente.getFuncao_acao()==null || componente.getFuncao_acao().equals("")) {
				continue;
			}



			try {

				FileReader arq = new FileReader(caminho_arquivo_interface_cpp);
				BufferedReader lerArq = new BufferedReader(arq);

				String linha = lerArq.readLine(); 
				String pega_nome = "";

				while (linha != null) {


					linha = linha.toLowerCase();

					String[] palavras = new String[10];

					if(linha.contains("void") && linha.contains(componente.getFuncao_acao())) {
						if(linha.contains("{")) abre_colchetes = abre_colchetes+1;
						to_na_funcao = true;
						linha = lerArq.readLine();
						continue;  
					}





					if(to_na_funcao==true) {

						if(linha.contains("{")) abre_colchetes = abre_colchetes + 1;
						if(linha.contains("}")) fecha_colchetes = fecha_colchetes + 1;
						if(abre_colchetes-fecha_colchetes==0) break;
						if(abre_colchetes>fecha_colchetes) { 
							if(linha.contains("reject()")) {
								//System.out.println(linha);
								componente.setTipo_no(tipo_no.terminal);
								x = x + 1;
								break;
							}
						}

					}

					linha = lerArq.readLine();    


				}	  
				arq.close();
			} catch (IOException e) {
				System.err.printf("Erro na abertura do arquivo: %s.\n Classe: Window_qt metodo: identificar_no_terminal()",
						e.getMessage());
			}


		}   
		return x;
	}


	public void imprimirComponentes() {

		System.out.println("Quantidade de componentes: "+ this.getComponentes().size());
		System.out.println();

		for(Componente componente : componentes) {

			System.out.println("\t Nome: "+componente.getNome_variavel());
			System.out.println("\t Tipo de no: "+componente.getTipo_no());
			System.out.println();

		}
		System.out.println("------------------------------");

	}


	public boolean verificar_existencia(String nome){
		for(Componente componente : componentes){

			if(componente.getNome_variavel().equals(nome)){

				return true;
			}
		}
		return false;
	}









}
