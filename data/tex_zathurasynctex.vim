" See LICENSE file for license and copyright information

" avoid re-execution
if exists("b:did_zathura_synctex_plugin")
  finish
endif
let b:did_zathura_synctex_plugin = 1

" set up global variables
if !exists("g:zathura_synctex_latex_suite")
  let g:zathura_synctex_latex_suite = 1
endif

function! Zathura_SyncTexForward()
  let source = expand("%:p")
  let input = shellescape(line(".").":".col(".").":".source)
  let output = "<none>"
  if exists("*Tex_GetMainFileName") && g:zathura_synctex_latex_suite == 1
    " use Tex_GetMainFileName from latex-suite if it is available
    let output = Tex_GetMainFileName(":p:r").".pdf"
  else
    " try to find synctex files and use them to determine the output file
    let synctex_files = split(glob("%:p:h/*.synctex.gz"), "\n")
    if len(synctex_files) == 0
      echo "No synctex file found"
      return
    endif

    let found = 0
    for synctex in synctex_files
      let pdffile = substitute(synctex, "synctex.gz", "pdf", "")
      let out = system("synctex view -i ".input." -o ".shellescape(pdffile))
      if match(out, "No tag for ".source) >= 0
        continue
      endif

      let found = 1
      let output = pdffile
      break
    endfor

    if found == 0
      echo "No synctex file containing reference to source file found"
      return
    endif
  endif

  let execstr = "zathura --synctex-forward=".input." ".shellescape(output)
  silent call system(execstr)
endfunction

nmap <buffer> <Leader>f :call Zathura_SyncTexForward()<Enter>
