{ pkgs ? import <nixos-unstable> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    bear
    postgresql
    paho-mqtt-c
  ];
}
