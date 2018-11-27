" SPDX-License-Identifier: Zlib

" This is a sample plugin that can be used for synctex forward synchronization.
" It currently uses latexsuite to obtain the file name of the document. If you
" are not using latexsuite, it should be enough to adopt the calculation of
" 'output' accordingly.

" avoid re-execution
if exists("b:did_zathura_synctex_plugin") || !exists("*Tex_GetMainFileName")
  finish
endif
let b:did_zathura_synctex_plugin = 1

function! Zathura_SyncTexForward()
  let source = expand("%:p")
  let input = shellescape(line(".").":".col(".").":".source)
  let output = Tex_GetMainFileName(":p:r").".pdf"
  let execstr = "zathura --synctex-forward=".input." ".shellescape(output)
  silent call system(execstr)
endfunction

nmap <buffer> <Leader>f :call Zathura_SyncTexForward()<Enter>
