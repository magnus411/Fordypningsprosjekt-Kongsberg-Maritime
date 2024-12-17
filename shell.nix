{ pkgs ? import <nixos> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    bear
    postgresql
    paho-mqtt-c
  ];
}
