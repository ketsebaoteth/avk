import re
import sys
import os

def parse_header_and_generate_charset(header_path, output_path):
    if not os.path.exists(header_path):
        print(f"Error: Header file not found at '{header_path}'")
        sys.exit(1)

    with open(header_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Matches patterns like U'\U0000E585' or U'\uE585'
    pattern = re.compile(r"=\s*U?'(?:\\U([0-9a-fA-F]{8})|\\u([0-9a-fA-F]{4}))'")
    
    codepoints = []
    seen = set()

    for match in pattern.finditer(content):
        hex_str = match.group(1) or match.group(2)
        if hex_str:
            cp_int = int(hex_str, 16)
            if cp_int not in seen:
                seen.add(cp_int)
                codepoints.append(f"0x{cp_int:X}")

    # Write ONLY the codepoints with no comment headers
    with open(output_path, 'w', encoding='utf-8') as f:
        for cp in codepoints:
            f.write(f"{cp}\n")

    print(f"Successfully extracted {len(codepoints)} unique icon codepoints to '{output_path}'.")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python genUnicodeForMsdf.py <path_to_lucideIcons.generated.h> <path_to_output_charset.txt>")
        sys.exit(1)
    
    parse_header_and_generate_charset(sys.argv[1], sys.argv[2])
