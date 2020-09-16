{ pkgs ? import <nixpkgs> {} }:

with pkgs;

stdenv.mkDerivation rec {
  pname = "trackpoint-clusters";
  version = "1.0.0";

  src = ./.;

  buildInputs = [ x11 xdotool ];
  propagatedBuildInputs = with xorg; [ xinput xmodmap perl ];

  buildPhase = ''
    gcc trackpoint-clusters.c -lX11 -lxdo -o ${pname}
  '';

  installPhase = ''
    mkdir -p $out/bin
    cp ${pname} $out/bin
  '';

  meta = with stdenv.lib; {
    description = "program that will reinterpret trackpoint click events as various modifier keys";
    longDescription = ''
      Remaps your trackpoint buttons to various modifier keys so that you can use them as thumb clusters.
      Left Click                  - Ctrl
      Right Click                 - Shift
      Right + Middle Click        - Level3 Shift
      Left + Middle Click         - Level5 Shift
      Will activate when you stop moving your trackpoint, and deactivate (and reenable your mouse buttons)
      when you move the trackpoint.
      Best used with xmodmap to remap how Level3 & Level5 shifts are interpreted.
    '';
    license = licenses.agpl3;
    maintainers = with maintainers; [ taeer ];
    platforms = platforms.linux;
  };
}
