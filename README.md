# C_compiler

## Project Overview
The modern C compiler replicates what current compilers(Klang, GDB, Swift) use to speed up C code. This is done through a process of translating C code into simpler languages, built with a specific optimization in mind, going from C->LB->LA->IR->L3->L2->L1 before being translated to assembly and machine code. 

## Languages
### L1
L1 is a barebones language that serves as a middle ground between the back-end compiler and assembly. The language definition is very similar to assembly.
   <div align="center">
      <img src="https://i.imgur.com/n27YT8t.png" alt="L1 Language">
      <br>
      <p>Figure 1: L1 Language Syntax</p>
  </div>

### L2
In L2, the language supports both variables and registers. The focus of this language is to map variables to registers. This is done through graph coloring, where a connection between two variables represents that they must be colored with two different registers. This graph is built using liveness analysis. For every line, we create a list of variables read and stored. If a variable is read, previously stored variables cannot share the same register. If graph coloring is not possible with 16 registers, we will "spill" the variable name, allocating the variable's value onto the stack. L2's syntax is built to support this:
   <div align="center">
      <img src="https://i.imgur.com/eCM6fEu.png" alt="L2 Language">
      <br>
      <p>Figure 2: L2 Language Syntax</p>
  </div>

### L3 
L3 is about instruction selection, combining multiple lines of code into one for optimizations. To do so, L3 merges instructions into large tree expressions:
   <div align="center">
      <img src="https://i.imgur.com/guczn3e.png" alt="L3 tree">
      <br>
      <p>Figure 3: L3 Tree</p>
  </div>
Then, we merge the tree to simplify the total number of expressions and convert them to L2 instructions. Thus simplifying the overall number of instructions. 
   <div align="center">
      <img src="https://i.imgur.com/7WSDdrF.png" alt="L3 merging trees">
      <br>
      <p>Figure 4: L3 merging trees</p>
  </div>
This is supported by L3's simpler syntax:
   <div align="center">
      <img src="https://i.imgur.com/hykeLkW.png" alt="L3 syntax">
      <br>
      <p>Figure 5: L3 language syntax</p>
  </div>

### IR
IR is not part of the optimization, but rather a simplified view between the front-end and back-end. This is reflected in the syntax, as its forceful use of basic blocks makes programs easier to read during debugging. 
   <div align="center">
      <img src="https://i.imgur.com/R5Vl5SB.png" alt="IR syntax">
      <br>
      <p>Figure 6: IR language syntax</p>
  </div>
  However, there is a problem going from IR to L3, due to IR's unique syntax. This is solved with control flow graph linearization. Naively we could have each false branch fall through to a force branch. However, we implemented CFG which utilized Markov chains which maximizes fall-through links while minimizing jumps.

### LA
An underlying problem throughout the previous compilers was how to differentiate a data value from a memory location. Previously, data members ended with a "1" bit while memory locations ended with a "0" bit. In LA, values can only be data members or an array or tuple of data members. In addition to this quick fix, we also implemented automatic tensor errors and type declarations.
   <div align="center">
      <img src="https://i.imgur.com/FFNcudL.png" alt="LA syntax">
      <br>
      <p>Figure 7: LA language syntax</p>
  </div>

### LB
Finally, in LB, we added scopes, which delimit variable names and allow for the creation of "if" and "while" statements.
   <div align="center">
      <img src="https://i.imgur.com/OcYXBHt.png" alt="LB syntax">
      <br>
      <p>Figure 8: LB language syntax</p>
  </div>

## Conclusion
In the end, our compiler was able to compete with up to 90% of the modern-day compilers. The reason why it was slower was due to modern compilers further optimizing spilling. 

All pictures are credited to Professor Simone Campanoni
