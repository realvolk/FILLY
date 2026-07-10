from setuptools import setup, find_packages

setup(
    name="filly-graphical",
    version="0.1.0",
    description="GTK4 graphical backend for FILLY",
    packages=find_packages(),
    install_requires=[
        "pygobject>=3.42.0",
        "jsonschema>=4.0.0",
    ],
    entry_points={
        "console_scripts": [
            "filly-graphical = filly_graphical.cli:main",
        ],
    },
)