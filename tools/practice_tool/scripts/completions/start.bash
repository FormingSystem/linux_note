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
        --uninstall)
            COMPREPLY=($(compgen -W 'minimal clean' -- "$current"))
            ;;
        *)
            COMPREPLY=($(compgen -W '-h --help --install --upgrade --host --port --completion --install-completion --uninstall' -- "$current"))
            ;;
    esac
}

complete -F _practice_start_completion start.sh
complete -F _practice_start_completion ./start.sh

_practice_install_completion() {
    local current="${COMP_WORDS[COMP_CWORD]}"
    COMPREPLY=($(compgen -W '-h --help --if-needed --upgrade --runtime-only' -- "$current"))
}

_practice_run_completion() {
    local current="${COMP_WORDS[COMP_CWORD]}"
    local previous="${COMP_WORDS[COMP_CWORD-1]}"
    case "$previous" in
        --host) COMPREPLY=($(compgen -W '127.0.0.1 0.0.0.0 localhost' -- "$current")) ;;
        --port) COMPREPLY=($(compgen -W '5173 5174 4173' -- "$current")) ;;
        *) COMPREPLY=($(compgen -W '-h --help --host --port' -- "$current")) ;;
    esac
}

_practice_uninstall_completion() {
    local current="${COMP_WORDS[COMP_CWORD]}"
    COMPREPLY=($(compgen -W '-h --help --minimal --clean --yes' -- "$current"))
}

complete -F _practice_install_completion install.sh ./install.sh
complete -F _practice_run_completion run.sh ./run.sh
complete -F _practice_uninstall_completion uninstall.sh ./uninstall.sh
