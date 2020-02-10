set nocompatible              " be iMproved, required
filetype off                  " required

so /root/.vim/plugins/plugins.vim

syntax on

let mapleader=','

"=============================== VISUALS ======================================="
colorscheme zellner      "Instructions: Make ~/.vim/colors dir and cd to it, 
                         "Find desired theme and get raw text URL 
                         "(Ex. https://raw.githubusercontent.com/tomasr/molokai/master/colors/molokai.vim). 
                         "Enter desired theme here, save, and source ~/.vimrc"

set t_CO=256             "Use 256 colors. This is for Terminal Vim.
set nu
set tabstop=4
set shiftwidth=4
set expandtab


"============================== SEARCHING ======================================"

set hlsearch
set incsearch

"============================== EDITING ========================================"

set backspace=indent,eol,start                    "Make backspace delete"

"============================== NAVIGATION ======================================"

nmap <C-I> <C-W><C-W>bw
nmap <Leader>sp :sp<CR>
nmap <Leader>vsp :vsp<CR>
nmap <Leader>p :NERDTreeToggle<CR>
set splitbelow
set splitright

"============================== MAPPINGS ======================================="

nmap <Leader>ev :vsp<CR>:e $MYVIMRC<CR>
nmap <Leader>ep :vsp<CR>:e ~/.vim/plugins/plugins.vim<CR>
nmap <C-R> :CtrlPBufTag<cr>
nmap <D-E> :CtrlPMRU<cr>
nmap <Leader><space> :nohlsearch<CR>

"============================== AUTO-SOURCE ====================================="

let g:ctrlp_custom_ignore = 'tap|angularjs|.git|.vim'
let g:ctrlp_match_window = 'bottom, order::ttb,min1,max30,results:30'

"============================== AUTO-SOURCE ====================================="

augroup autosourcing                              "Remake autocmd on Save"
    autocmd!
    autocmd BufWritePost ~/.vimrc source %
    autocmd BufRead Makefile setlocal noexpandtab
augroup END














