#!/usr/bin/env python3
"""Writes BuildOptionsMSVC.inc, the rows of Edit Build Configurations' MSVC page (BuildOptionEdits.cpp; HCP65-IDE's
ConfigurationsPlan.txt, C9 stage 3), from Visual Studio's own descriptions of its property pages:

    python3 MakeMSVCOptions.py <MSBuild>/Microsoft/VC/v160/1033 > BuildOptionsMSVC.inc

cl.xml, link.xml and lib.xml there say every setting of the compiler, the linker and the librarian: its tag, the name
its page shows, the page's category it is in, and for one with fixed values what they are.  Each is a row: one that is
true or false or has values is chosen from them, a list adds up (BuildOptionEdits.h, bAdds), and anything else is typed.

Left out: a setting the pages don't show (Visible="false"), and one kept as a property of the project, not in the
tool's group (it has a data source of its own: LinkIncremental), which the dialog doesn't write yet.  The file is
written once and committed: run this again for another version of Visual Studio.
"""
import os
import sys
import textwrap
import xml.etree.ElementTree as ET

RULES = (("cl.xml", "ClCompile", "C/C++", False), ("link.xml", "Link", "Linker", True), ("lib.xml", "Lib", "Librarian", False))
KINDS = ("StringListProperty", "BoolProperty", "EnumProperty", "StringProperty", "IntProperty", "DynamicEnumProperty")


def quoted(text):
	return '"%s"' % text.replace("\\", "\\\\").replace('"', '\\"')


def rows(folder):
	out = []
	left = []
	for file, tool, page, program in RULES:
		root = ET.parse(os.path.join(folder, file)).getroot()
		ns = root.tag[:root.tag.index("}") + 1]
		categories = {}
		for category in root.iter(ns + "Category"):
			categories[category.get("Name")] = category.get("DisplayName") or category.get("Name")
		for element in root:
			kind = element.tag.replace(ns, "")
			if kind not in KINDS:
				continue
			name = element.get("Name")
			if element.get("Visible") == "false" or element.find(ns + kind + ".DataSource") is not None:
				left.append("%s.%s" % (tool, name))
				continue
			category = categories.get(element.get("Category"), element.get("Category") or "General")
			label = element.get("DisplayName") or name
			adds, separator, choices = "false", "0", "NULL"
			if kind == "StringListProperty" or name == "AdditionalOptions":
				adds = "true"
				separator = "' '" if name == "AdditionalOptions" else "';'"
			elif kind == "BoolProperty":
				choices = quoted("false;true")
			elif kind == "EnumProperty":
				choices = quoted(";".join(value.get("Name") for value in element.findall(ns + "EnumValue")))
			section = page if len(categories) <= 3 else "%s: %s" % (page, category)
			out.append('\t{ %s, %s, %s, %s, false, "", BOK_MSVC, %s, %s, %s, %s },' % (quoted(tool), quoted(name), quoted(label), "true" if program else "false", adds, separator, choices, quoted(section)))
	return out, left


def main(arguments):
	if len(arguments) != 1:
		print(__doc__)
		return 1
	out, left = rows(arguments[0])
	print("//Written by MakeMSVCOptions.py from Visual Studio's cl.xml, link.xml and lib.xml: %d settings.  Not by hand." % len(out))
	print("//Left out, each kept as a property or not shown by the pages:")
	for line in textwrap.wrap(", ".join(left) + ".", 112, break_long_words=False):
		print("//  " + line)
	print("\n".join(out))
	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
