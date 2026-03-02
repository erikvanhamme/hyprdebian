# ~/.bash_aliases

alias ls='eza --icons=auto'
alias ll='eza -alg --group-directories-first --icons=auto'

alias y='yazi'

alias wav2flac='find . -type f -name *.wav -exec ffmpeg -i {} -af aformat=s16:44100 {}.flac \; -delete'
