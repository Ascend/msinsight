#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import re


def read_file(file_path):
    with open(file_path, 'r', encoding='utf-8') as f:
        return f.read()


def write_file(file_path, content):
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(content)


def convert_path(path):
    if path.startswith('http://') or path.startswith('https://') or path.startswith('#'):
        return path
    if path.startswith('./docs/zh/'):
        return './zh/' + path[10:]
    if path.startswith('./'):
        return 'https://raw.gitcode.com/Ascend/msinsight/files/master/' + path[2:]
    return path


def replace_path(match):
    return match.group(1) + convert_path(match.group(2)) + match.group(3)


def convert_paths(content):
    content = re.sub(r'\./zh/', './', content)
    content = re.sub(r'(\]\()([^)]+)(\))', replace_path, content)
    content = re.sub(r'(src=")([^"]+)(")', replace_path, content)
    return content


def remove_trailing_blank_lines(lines):
    while lines and lines[-1].strip() == '':
        lines.pop()


def should_remove_details(lines, line_index, titles_to_remove):
    if '<details>' not in lines[line_index]:
        return False
    next_line_index = line_index + 1
    if next_line_index >= len(lines):
        return False
    summary_match = re.search(r'<summary[^>]*>(.+?)</summary>', lines[next_line_index])
    return bool(summary_match and summary_match.group(1).strip() in titles_to_remove)


def remove_sections(content, titles_to_remove):
    lines = content.split('\n')
    result_lines = []
    skip = False
    current_level = 0
    skip_details = False

    for line_index, line in enumerate(lines):
        if should_remove_details(lines, line_index, titles_to_remove):
            skip = True
            skip_details = True
            remove_trailing_blank_lines(result_lines)
            continue

        if skip_details and '</details>' in line:
            skip = False
            skip_details = False
            continue

        header_match = re.match(r'^(#{1,6})\s+(.+)$', line)

        if header_match:
            level = len(header_match.group(1))
            title = header_match.group(2).strip()

            if title in titles_to_remove:
                skip = True
                current_level = level
                remove_trailing_blank_lines(result_lines)
                continue
            if skip and level <= current_level:
                skip = False

        if skip:
            continue

        result_lines.append(line)

    return '\n'.join(result_lines)


def main():
    titles_to_remove = []

    readme_path = 'README.md'
    index_path = 'docs/index.md'

    readme_content = read_file(readme_path)
    index_content = read_file(index_path)

    if titles_to_remove:
        readme_content = remove_sections(readme_content, titles_to_remove)

    processed_readme = convert_paths(readme_content)

    new_index_content = processed_readme.rstrip() + '\n\n' + '\n'.join(index_content.splitlines()[1:])

    write_file(index_path, new_index_content)

    print(f"Successfully processed README.md and updated {index_path}")


if __name__ == '__main__':
    main()
