# ~/.bash_aliases

alias ls='eza --icons=auto'
alias ll='eza -alg --group-directories-first --icons=auto'

alias y='yazi'

alias wav2flac='find . -type f -name *.wav -exec ffmpeg -i {} -af aformat=s16:44100 {}.flac \; -delete'

# fzf history search (Ctrl-R)

__fzf_history__() {
  local selected
  selected=$(history | tac | sed 's/ *[0-9]* *//' | fzf --height 75% --reverse --border --color=dark) || return
  READLINE_LINE="$selected"
  READLINE_POINT=${#READLINE_LINE}
}

bind -x '"\C-r": __fzf_history__'
