# ~/.bash_aliases

alias y='yazi'

alias wav2flac='find . -type f -name *.wav -exec ffmpeg -i {} -af aformat=s16:44100 {}.flac \; -delete'
