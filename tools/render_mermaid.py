import requests
import sys

def render_mermaid_to_file(input_file, output_svg, output_png):
    with open(input_file, 'r') as file:
        mermaid_code = file.read()

    url = "https://kroki.io/mermaid/svg"
    response_svg = requests.post(url, data=mermaid_code, headers={"Content-Type": "text/plain"})

    if response_svg.status_code == 200:
        with open(output_svg, 'wb') as svg_file:
            svg_file.write(response_svg.content)
    else:
        print("Failed to render SVG:", response_svg.text)
        sys.exit(1)

    url = "https://kroki.io/mermaid/png"
    response_png = requests.post(url, data=mermaid_code, headers={"Content-Type": "text/plain"})

    if response_png.status_code == 200:
        with open(output_png, 'wb') as png_file:
            png_file.write(response_png.content)
    else:
        print("Failed to render PNG:", response_png.text)
        sys.exit(1)

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: python render_mermaid.py <input_file> <output_svg> <output_png>")
        sys.exit(1)

    input_file = sys.argv[1]
    output_svg = sys.argv[2]
    output_png = sys.argv[3]

    render_mermaid_to_file(input_file, output_svg, output_png)