_practice_start_completion() {
    local current previous
    current="${COMP_WORDS[COMP_CWORD]}"
    previous="${COMP_WORDS[COMP_CWORD-1]}"

    case "$previous" in
        --host)
            COMPREPLY=($(compgen -W '127.0.0.1 0.0.0.0 localhost' -- "$current"))
            ;;
        --port)
            COMPREPLY=($(compgen -W '5173 5174 4173' -- "$current"))
            ;;
        --completion)
            COMPREPLY=($(compgen -W 'bash' -- "$current"))
            ;;
        *)
            COMPREPLY=($(compgen -W '-h --help --upgrade --host --port --completion --install-completion' -- "$current"))
            ;;
    esac
}

complete -F _practice_start_completion start.sh
complete -F _practice_start_completion ./start.sh
