package leitura_interface;


import org.jgrapht.*;
import org.jgrapht.graph.*;
import org.jgrapht.io.ComponentNameProvider;
import org.jgrapht.io.DOTExporter;
import org.jgrapht.io.ExportException;
import org.jgrapht.io.GraphExporter;
import org.jgrapht.traverse.*;

import java.io.*;
import java.net.*;
import java.util.*;
public class Main {


	/////// ALTERAR OS PARAMETROS AQUI
	private static int estudo_caso = 1;
	private static String cfgProp = "/home/juliana/eclipse-workspace/Doutorado/config.properties";
	private static String TEMP_DIR = "/home/juliana/eclipse-workspace/Doutorado/temp"; //  The dir. where temporary files will be created.
	private static String cpp_path = "/home/juliana/eclipse-workspace/Doutorado/arquivos_para_leitura/"+estudo_caso+"/main.ui";
	private static String path_graph = "/home/juliana/eclipse-workspace/Doutorado/estudos_caso/"+estudo_caso;
	private static String ui_path =  "/home/juliana/eclipse-workspace/Doutorado/arquivos_para_leitura/"+estudo_caso+"/main.cpp"; 
	private static String path_output = "/home/juliana/eclipse-workspace/Doutorado/dots/"+estudo_caso+".dot";
	

	public static void main(String[] args) throws URISyntaxException, ExportException, IOException{

		//QT


		// alterar aqui o numero do exercicio 
		int exe = estudo_caso;

	
		Interface teste = Interface.getInstance();
		teste.SetBiblioteca(biblioteca_interface.qt);


		teste.setCaminho_arquivo_interface(cpp_path, ui_path);

		teste.lerComponentes();
		EFG efg = EFG.getInstance();
		efg.construirEFG();
		String formato_dot = efg.exportarEFGDot();
		efg.imprimirArquivo(path_output, formato_dot);

		createDotGraph(formato_dot, (path_graph));
	}	


	public static void createDotGraph(String dotFormat,String fileName)
	{

		GraphViz gv = new GraphViz(cfgProp, TEMP_DIR);
		gv.addln(gv.start_graph());
		gv.add(dotFormat);
		gv.addln(gv.end_graph());
		String type = "pdf";
		gv.decreaseDpi();
		gv.decreaseDpi();
		File out = new File(fileName+"."+ type); 
		gv.writeGraphToFile( gv.getGraph( gv.getDotSource(), type ), out );
	}



}