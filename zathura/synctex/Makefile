FILES=synctex_parser.c synctex_parser.h synctex_parser_utils.c synctex_parser_utils.h synctex_parser_version.txt

.PHONY: update
update:
	set -e; for FILE in $(FILES) ; do \
		wget -O "$$FILE" "http://www.tug.org/svn/texlive/trunk/Build/source/texk/web2c/synctexdir/$$FILE?view=co" ; \
	done
