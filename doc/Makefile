FIG_SRC=$(wildcard figure/*.tex)
FIG_PDF=$(FIG_SRC:.tex=.pdf)
FIG=$(FIG_SRC:.tex=.png)

figure: $(FIG) $(FIG_PDF)

figure/%.png: figure/%.pdf
	pdftoppm $< $(basename $@) -singlefile -png -rx 300 -ry 300

figure/%.pdf: figure/%.tex
	latexmk -cd $<
	latexmk -cd -c $<