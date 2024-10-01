{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell {
  buildInputs = with pkgs.pkgsi686Linux; [
    glibc
    stdenv
    pkgs.strace
  ];
}
