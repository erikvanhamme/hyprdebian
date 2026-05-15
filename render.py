import argparse
import os
from jinja2 import Template

def render_template():
    parser = argparse.ArgumentParser(description="Generic Jinja2 Renderer")
    parser.add_argument("template", help="Path to the Jinja2 template file")
    parser.add_argument("output", help="Path to the output file")
    parser.add_argument("-v", "--var", action="append", help="Variables in key=value format")
    parser.add_argument("-m", "--mode", default="0644", help="Octal file mode (default: 0644)")

    args = parser.parse_args()

    context = {}
    if args.var:
        for item in args.var:
            key, value = item.split("=", 1)
            if value.lower() == "true":
                value = True
            elif value.lower() == "false":
                value = False
            context[key] = value

    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)

    with open(args.template, "r") as f:
        template = Template(f.read())

    rendered_content = template.render(context)

    # Use os.O_WRONLY | os.O_CREAT | os.O_TRUNC to open the file 
    # and set the mode during the initial creation.
    # int(args.mode, 8) converts the string "0600" to its octal integer.
    mode = int(args.mode, 8)
    with os.fdopen(os.open(args.output, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, mode), 'w') as f:
        f.write(rendered_content)

    print(f"Successfully rendered {args.output} with mode {args.mode}")

if __name__ == "__main__":
    render_template()
