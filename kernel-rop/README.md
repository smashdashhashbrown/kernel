# General Kernel Protections and Kernel-ROP Writeup

## Dependencies

## Instructions

## Introduction

## Kernel Protections and How to Exploit

### SMEP/SMAP

### KPTI

### KASLR/FG-KASLR


cat /proc/kallsyms | grep -E "prepare_kernel_cred|commit_creds"
ffffffffa5ef7810 T prepare_kernel_cred
ffffffffa619ee20 T commit_creds
ffffffffa6987d90 r __ksymtab_commit_creds
ffffffffa698d4fc r __ksymtab_prepare_kernel_cred
ffffffffa69a0972 r __kstrtab_commit_creds
ffffffffa69a09b2 r __kstrtab_prepare_kernel_cred
ffffffffa69a4d42 r __kstrtabns_prepare_kernel_cred
ffffffffa69a4d42 r __kstrtabns_commit_creds
