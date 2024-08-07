build:
	python setup.py build
clean:
	python setup.py clean
	rm -r ./dist/
	rm -r ./HLL.egg-info/
install:
	python setup.py install
remove:
	python setup.py uninstall HLL
sdist:
	python setup.py sdist
test_sdist:
	pip install .
upload:
	twine upload dist/*
