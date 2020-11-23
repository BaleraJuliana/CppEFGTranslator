# CppEFGTranslator :computer:

Project status: complete :heavy_check_mark:

This software translates Qt-extended C++ code into an Event Flow Graph (EFG) to support Model-Driven Development. 

## Running :arrow_forward:

In order to run, consider the following steps:

1. The input files are "cpp" and "ui". These input files should be placed in the "read_archives" folder, following the structure according to the examples present within that folder.

2. Create the "case_studies" and "dots" folders in the project root. The output files will be in these folders.

3. Add the files related to a case study into the "read_archives" folder.

4. Adapt the paths in the "Main" class.

5. Select the case study by modifying the "estudo_caso" variable.

6. Run the program.

7. After the execution is finished, the "dots" folder will have the EFG in .dot file format for the selected case study and, in the "case_studies" folder, the EFG is in .pdf format.  

## Author :busts_in_silhouette:

Juliana Marino Balera

## Licence  

This project is licensed under the GNU GENERAL PUBLIC LICENSE, Version 3 (GPLv3) - see the LICENSE.md file for details.
