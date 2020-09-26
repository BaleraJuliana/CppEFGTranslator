package leitura_interface;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;

public class EFG {
	
	
	public static EFG uniqueInstance;
	private Interface minha_interface;

	private EFG() {
		minha_interface = Interface.getInstance();
	}
	
	public static EFG getInstance() {
		if(uniqueInstance==null) {
			uniqueInstance = new EFG();
		}
		return uniqueInstance;
	}
	
	public void construirEFG() {
		
		int componentes = 0;
		for(Window janela : minha_interface.getJanelas()) {
			for(Componente componente : janela.getComponentes()) {
				componentes = componentes + 1;
			}
		}
		
		for(Window janela : minha_interface.getJanelas()) {
			
			if(janela.getComponentes().size()==0) continue;
			int no_comparado = 0;
			int itens_interface = janela.getComponentes().size();
			int i = 1;
			
			
			for(Componente componente : janela.getComponentes()) {
				
				i = 0;
				
				while(i<itens_interface){
					if(componente.getTipo_no()==tipo_no.medio) 
						
						
						if((janela.getComponentes().get(i).getTipo_no()==tipo_no.r_value)==false) {
							componente.addNo(janela.getComponentes().get(i));
						} 
					
					i = i + 1;
				}	
				no_comparado = no_comparado + 1;
				
			}	

		}
		
			}

	public void imprimirEFG() {
		
		System.out.println("imprimindo EFG...");
		for(Window janela : minha_interface.getJanelas()) {
			System.out.println("EFG "+janela.getNome());
			for(Componente componente : janela.getComponentes()) {
				System.out.println("No: "+componente.getNome_variavel()+" aponta para: ");
				if(componente.getNos_que_saem().size()==0) {
					System.out.println("nada");
				}
				for(Componente no : componente.getNos_que_saem()) {
					
					System.out.println("\t -> "+no.getNome_variavel()+ " " + no.getFuncao_acao());
				}
				System.out.println("");
			}
			System.out.println("-----------------------");
		}
	}
	
	

	public String exportarEFGDot() {
		
		String saida = "strict digraph G {"+"\n";
		int con = 0;
		for(Window janela : minha_interface.getJanelas()) {
			for(Componente componente : janela.getComponentes()) {
				saida = saida +"\t"+componente.getNome_variavel()+";"+"\n";
			}
		}
		
		for(Window janela : minha_interface.getJanelas()) {
			for(Componente componente : janela.getComponentes()) {
				for(Componente no : componente.getNos_que_saem()) {
					saida = saida + "\t"+componente.getNome_variavel()+" -> "+no.getNome_variavel()+";"+"\n";
				}
			}
		}
		
		saida = saida + "}";
		return saida;
	}
	
	
	public void imprimirArquivo(String caminho_arquivo, String conteudo) throws IOException{

		String caminho = caminho_arquivo;
		
		FileWriter saida = new FileWriter(caminho);
		PrintWriter gravarArquivo = new PrintWriter(saida);

		int id_teste = 0; 
		gravarArquivo.print(conteudo);
		saida.close();
	}
	
	
	public Interface getInterface() {
		return minha_interface;
	}
}
