#!/bin/sh
#
#
# CLI Help function
#
show_help() {
cat << EOF
Usage: ${0##*/} [-h] [-s logical-switch] [-v logical VNF port] [-a logical qpplication port] [-d display current state] [-e echo the porposed commands] [-f dump flows after commands are applied] [-c clear configuration]
 
     -h          display this help and exit
     -s          logical switch
     -v          vnf logical port
     -a          application logical port
     -d          display current chain classifier and exit
     -e          echo the commands to be applied and exit without applying
     -f          dump sb flows
     -c          clear configuration
EOF
}
#
# Parse Command Line Arguments
#
while getopts "hcdefs:v:a:" opt; do
    case "$opt" in
        h)
            show_help
            exit 0
            ;;
        s) opt_ls=$OPTARG
           ;;
        v) opt_lsp_vnf=$OPTARG
           ;;
        a) opt_lsp_app=$OPTARG
           ;;
        d) opt_d=true
           ;;
        e) opt_e=true
           ;;
        f) opt_f=true
           ;;
        c) opt_c=true
           ;;
        '?')
            show_help
            exit 1
            ;;
    esac
done

if [ ! -z "$opt_d" ]; then
        printf "\nCurrent system state\n\n"
        ovn-nbctl lsp-chain-show
        ovn-nbctl lsp-chain-classifier-show
        exit 1
fi

if [ ! -z "$opt_c" ]; then
        printf "\nClearing existing configuration\n"
        ovn-nbctl --if-exists lsp-chain-classifier-del pcc1
        ovn-nbctl --if-exists lsp-pair-group-del ppg1
        ovn-nbctl --if-exists lsp-pair-del pp1
        ovn-nbctl --if-exists lsp-chain-del pc1
        exit 1
fi

if [ -z "$opt_ls" ]; then
        printf "\nERROR: Logical switch must be defined\n"
        exit 1
fi

if [ -z "$opt_lsp_vnf" ]; then
        printf "\nERROR: Logical switch port for VNF must be defined\n"
        exit 1
fi

if [ -z "$opt_lsp_app" ]; then
        printf "\nERROR: Logical switch port for application must be defined\n"
        exit 1
fi

if [ ! -z "$opt_e" ]; then
        printf "\nEcho candidate config\n"
        printf "\novn-nbctl lsp-pair-add  $opt_ls $opt_lsp_vnf $opt_lsp_vnf pp1"
        printf "\novn-nbctl lsp-chain-add $opt_ls pc1"
        printf "\novn-nbctl lsp-pair-group-add pc1 ppg1"
        printf "\novn-nbctl lsp-pair-group-add-port-pair ppg1 pp1"
        printf "\novn-nbctl lsp-chain-classifier-add $opt_ls pc1 $opt_lsp_app \"exit-lport\" \"bi-directional\" pcc1 \"\"\n"
        exit 1
fi
#
# Default install new rules
#
# Remove all existing Configuration
ovn-nbctl --if-exists lsp-chain-classifier-del pcc1
ovn-nbctl --if-exists lsp-pair-group-del ppg1
ovn-nbctl --if-exists lsp-pair-del pp1
ovn-nbctl --if-exists lsp-chain-del pc1
#
# Configure Chain
#
#
# Configure Service chain
#
ovn-nbctl lsp-pair-add  $opt_ls $opt_lsp_vnf $opt_lsp_vnf pp1
ovn-nbctl lsp-chain-add $opt_ls pc1
ovn-nbctl lsp-pair-group-add pc1 ppg1
ovn-nbctl lsp-pair-group-add-port-pair ppg1 pp1
ovn-nbctl lsp-chain-classifier-add $opt_ls pc1 $opt_lsp_app "exit-lport" "bi-directional" pcc1 ""
#
if [ ! -z "$opt_f" ]; then
        ovn-sbctl dump-flows
fi
