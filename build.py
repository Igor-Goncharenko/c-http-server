import argparse
import os
import re
from typing import Iterable, List, Set, Tuple


def arg_parser_init() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
            prog="C single-header library builder")

    parser.add_argument(
            "--macro",
            nargs=1,
            required=True,
            help="Macro prefix"
            )
    parser.add_argument(
            "--output",
            nargs=1,
            required=True,
            help="Path to output file"
            )
    parser.add_argument(
            "--intro", 
            nargs="+", 
            default=[],
            help="Info files at the beginning of the library, "
            "e.g. Lisense or general info etc."
            )
    parser.add_argument(
            "--pub", 
            nargs=1, 
            required=True,
            help="Main library api (might be one main header"
            )
    parser.add_argument(
            "--src", 
            nargs="+",
            default=[],
            help="All source files of library"
            )
    parser.add_argument(
            "--private", 
            nargs="+",
            default=[],
            help="Private header files"
            )
    parser.add_argument(
            "--outro", 
            nargs="+",
            default=[],
            help="Info files at the end of the library,"
            "e.g. Lisense, Changelog etc."
            )

    return parser


def print_intro(intro_files: List[str]) -> str:
    if len(intro_files) == 0:
        return ""

    result: str = ""
    result += "/*\n"
    for filename in intro_files:
        with open(filename, "r") as fd:
            result += fd.read()
    result += "\n*/\n"

    return result


def print_pub(pub_file: str) -> str:
    with open(pub_file, "r") as fd:
        return fd.read()


def remove_includes(files: List[str], file_source: str
                    ) -> Tuple[str, List[str]]:
    """."""
    result_str: str = file_source
    # delete local includes
    for file in files:
        fname = os.path.basename(file)
        if fname.endswith(".h"):
            result_str = result_str.replace(f"#include \"{fname}\"\n", "")
            result_str = result_str.replace(f"#include <{fname}>\n", "")
    # delete standard library includes
    stdlib_inc = re.findall(r"#include <[a-zA-Z_][a-zA-Z0-9_/]*\.h>\n", result_str)
    stdlib_inc += re.findall(r"#include \"[a-zA-Z_][a-zA-Z0-9_/]*\.h\"\n", result_str)
    for inc in stdlib_inc:
        result_str = result_str.replace(inc, "")

    return result_str, stdlib_inc


def sort_includes(includes: Iterable[str]) -> List[str]:
    nested_includes = list(filter(lambda x: "/" in x, includes))
    usual_includes = list(set(includes).difference(nested_includes))
    return sorted(usual_includes) + sorted(nested_includes)


def format_private_n_src_files(pub: str, private: List[str], source: List[str]) -> str:
    priv_src = private + source
    all_files = priv_src + [pub]
    result: str = ""

    stdlib_includes: Set[str] = set()

    for file in priv_src:
        with open(file, "r") as fd:
            file_src = fd.read()
            file_without_includes, stdlib_inc = \
                    remove_includes(all_files, file_src)

            stdlib_includes.update(stdlib_inc)
            result += file_without_includes

    sorted_includes = sort_includes(stdlib_includes)
    result = "".join(sorted_includes) + "\n" + result

    return result


def main() -> None:
    parser = arg_parser_init()
    args = parser.parse_args()
    macro = args.macro[0]

    with open(args.output[0], "w") as output_fd:
        output_fd.write(print_intro(args.intro))

        output_fd.write(f"#ifndef {macro}_SINGLE_FILE\n"
                        f"# define {macro}_SINGLE_FILE\n"
                        f"#endif\n\n")

        output_fd.write(print_pub(args.pub[0]))
        output_fd.write("\n")

        output_fd.write(f"#ifdef {macro}_IMPLEMENTATION\n\n")

        output_fd.write(
                format_private_n_src_files(args.pub[0], args.private, args.src)
                )

        output_fd.write(f"\n#endif /* {macro}_IMPLEMENTATION */\n")


if __name__ == "__main__":
    main()

