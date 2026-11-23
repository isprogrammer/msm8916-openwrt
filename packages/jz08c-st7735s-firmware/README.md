The firmware blob is generated from the stock `st7735s-panel-dtb01.txt` init
property using the mainline MIPI DBI version-1 command format. Qualcomm's source
byte 0 is the delay in milliseconds after the command is transmitted; a nonzero
delay is emitted as MIPI DBI NOP record `00 01 <delay>`.

Rebuild the committed blob with:

    python3 convert_mipi_dbi.py files/st7735s-panel-dtb01.txt files/sitronix,st7735s-jz08c-v51.bin
