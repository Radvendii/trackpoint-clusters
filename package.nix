{ stdenv
, lib
, makeWrapper
, coreutils
, perl
, xdotool
, xorg
}:

stdenv.mkDerivation rec {
  pname = "trackpoint-clusters";
  version = "1.0.1";

  src = ./.;

  nativeBuildInputs = [ makeWrapper ];

  buildInputs = [ xorg.libX11 xdotool xorg.libXfixes ];

  buildPhase = ''
    gcc ${pname}.c -lxdo -lXfixes -lX11 -o ${pname}
  '';

  installPhase = ''
    mkdir -p $out/bin
    cp ${pname} $out/bin

    # need runtime access to cat and perl
    wrapProgram $out/bin/${pname} \
      --prefix PATH : ${lib.makeBinPath [ coreutils perl ]}
  '';

  meta = with lib; {
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
    license = licenses.agpl3Plus;
    maintainers = with maintainers; [ taeer ];
    platforms = platforms.linux;
  };
}
