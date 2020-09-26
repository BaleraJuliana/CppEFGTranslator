package leitura_interface;

public class Biblioteca {
	
	private biblioteca_interface nome;
	private static Biblioteca uniqueInstance;
	
	
	private Biblioteca(){}
	
	public static Biblioteca getInstance(){
		if(uniqueInstance==null)
			return new Biblioteca();
		else
			return uniqueInstance;
	}


	public biblioteca_interface getNome() {
		return nome;
	}


	public void setNome(biblioteca_interface nome) {
		this.nome = nome;
	}


	public String getWindow() {
		if(nome==biblioteca_interface.gtk)
			return "gtk::window";
		
		return "qwindow";
		
	}


	public String getButton() {
		if(nome==biblioteca_interface.gtk)
			return "gtk::button";
		
		return "qpushbutton";
		
	}

	public String getRadiobutton() {
		if(nome==biblioteca_interface.gtk)
			return "gtk::radiobutton";
		
		return "qradiobutton";
		
	}

	public String getLabel() {
		if(nome==biblioteca_interface.gtk)
			return "gtk::label";
		return "qlabel";
	}

	public String getSpinbutton() {
		if(nome==biblioteca_interface.gtk)
			return "gtk::spinbutton";
		return "qspinbox";
	}

	public String getGrid() {
		if(nome==biblioteca_interface.gtk)
			return "gtk::grid";
		return "qgridlayout";
	}

	public String getQLineEdit() {
		if(nome==biblioteca_interface.gtk)
			return "gtk::lineedit";
		return "qlineedit";
	}

	public String getQListWidget() {
		if(nome==biblioteca_interface.gtk)
			return "gtk::listwidget";
		return "qlistwidget";
	}

	public String getQGroupBox() {
		if(nome==biblioteca_interface.gtk)
			return "gtk::groupbox";
		return "qgroupbox";
	}

	public String getQComboBox() {
		if(nome==biblioteca_interface.gtk)
			return "gtk::combobox";
		return "qcombobox";
	}

	public String getQSlider() {
		if(nome==biblioteca_interface.gtk)
			return "gtk::slider";
		return "qslider";
	}

	public String getQToolButton() {
		if(nome==biblioteca_interface.gtk)
			return "gtk::toolbutton";
		return "qtoolbutton";
	}

	public String getQCheckBox() {
		if(nome==biblioteca_interface.gtk)
			return "gtk::checkbox";
		return "qcheckbox";
	}



}
