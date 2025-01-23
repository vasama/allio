#!/usr/bin/env python3

import argparse
import os
import re
import textwrap

if __name__ == "__main__":
	parser = argparse.ArgumentParser(prog='generate-error-encoding')

	parser.add_argument("--encoding")
	parser.add_argument("--sources")
	parser.add_argument("--outfile")

	args = parser.parse_args()

	re_file_name = re.compile("\\ballio_error\\b")

	file_names = set()
	for source_file in args.sources.split(';'):
		with open(source_file, "r") as file:
			source_data = file.read()

		if re.search(re_file_name, source_data):
			file_names.add(source_file)

	def make_string_literals(strings):
		strings = list(strings)
		strings.sort()
		return "\n".join(map(lambda x: f'\t\t\t"{x}",', strings))

	file_content = textwrap.dedent(f"""\
	#pragma once

	struct {args.encoding}
	{{
		static constexpr char const* file_names[] =
		{{
{ make_string_literals(file_names) }
		}};
	}};
	""")

	file_existing_content = None
	if os.path.isfile(args.outfile):
		with open(args.outfile, "r") as file:
			file_existing_content = file.read()

	if file_content != file_existing_content:
		with open(args.outfile, "w") as file:
			file.write(file_content)
