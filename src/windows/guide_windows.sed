# Adapt only the copies of the guides placed in the Windows binary ZIP.
# The canonical Japanese/English source guides are never rewritten.
# Use a path accepted by both Command Prompt and PowerShell.
# A source TXT line is split over two lines; keep that original pagination.
s/^   JEC版では、標準のセーブファイルから「rogue -r」で再開すること$/   JEC版では、標準のセーブファイルから/
s/% rogue save_file/> .\\rogue34.exe save_file/g
s/% rogue -r/> .\\rogue34.exe -r/g
s/rogue -r/.\\rogue34.exe -r/g
# The packaged executables share the same default save-file name.
s/file（セーブファイル名）/file [rogue34.save]（セーブファイル名）/g
s/<span class="term-label">file<\/span>/<span class="term-label">file [rogue34.save]<\/span>/g
s/^file[[:space:]]*Name of the default save file\./file [rogue34.save]      Name of the default save file./
# Replace historical Unix shell examples with Windows commands.
s/% setenv ROGUEOPTS &quot;/> set \&quot;ROGUEOPTS=/g
s/% setenv ROGUEOPTS "/> set "ROGUEOPTS=/g
s/\$ ROGUEOPTS=&quot;/PS> \$env:ROGUEOPTS=\&quot;/g
s/\$ ROGUEOPTS="/PS> \$env:ROGUEOPTS="/g
s/\$ export ROGUEOPTS/PS> .\\rogue34.exe/g
s/For those of you who use the Bourne shell, the commands/For those using PowerShell, the commands/g
s/For those of you who use the Bourne shell, the commands would be/For those using PowerShell, the commands would be/g
s/Bourneシェルを使っている場合は/PowerShellを使っている場合は/g
s/is "mango", use the command/is "mango", in Command Prompt use the command/g
s/is &quot;mango&quot;, use the command/is &quot;mango&quot;, in Command Prompt use the command/g
s/次のように設定する。/Windowsのコマンドプロンプトでは次のように設定する。/g
# Keep the preformatted Japanese text inside its original 64-cell measure.
s/^ もできる。$/ 「.\\rogue34.exe -r」で再開することもできる。/
