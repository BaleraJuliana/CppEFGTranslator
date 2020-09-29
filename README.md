# CppEFGTraslator

Translator from Qt-extended C++ into Event Flow Graph (EFG).

A. Input files: "cpp" and "ui".
<br> <br>

B. Where you should place the input files: inside the "read_archives" folder,
                                                following the structure according to
                                                examples present within that folder.
<br>
C. Where will the output files be: inside the "dots" and "case_studies" folders.
<br>
Step by step...

- 1. Create the "case_studies" and "dots" folders in the project root.
- 2. Add the case studies to the "read_archives" folder.
- 3. Adpate the paths in the "Main" class.
- 4. Select the case study by modifying the "estudo_caso" variable.
- 5. Run the program.
- 6. After the execution is finished, if everything goes well, In the folder "dots"
    there will be a .dot file for the selected case study, and
    in the "case_studie" folder there will be a pdf referring to the generated pain graph.
