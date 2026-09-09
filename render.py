import argparse
import os
from jinja2 import Template

def parse_env_value(val: str):
    lowered = val.strip().lower()
    if lowered in ("true", "1", "yes", "on"):
        return True
    if lowered in ("false", "0", "no", "off"):
        return False
    if val.isdigit():
        return int(val)
    return val

def render_template():
    parser = argparse.ArgumentParser(description="Generic Jinja2 Renderer")
    parser.add_argument("template", help="Path to the Jinja2 template file")
    parser.add_argument("output", help="Path to the output file")
    parser.add_argument("-m", "--mode", default="0644", help="Octal file mode (default: 0644)")

    args = parser.parse_args()

    # Build a cleaned context dictionary from os.environ
    raw_context = dict(os.environ)
    clean_context = {k: parse_env_value(v) for k, v in raw_context.items()}

    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)

    with open(args.template, "r") as f:
        template = Template(f.read())

    rendered_content = template.render(clean_context)

    # Use os.O_WRONLY | os.O_CREAT | os.O_TRUNC to open the file 
    # and set the mode during the initial creation.
    # int(args.mode, 8) converts the string "0600" to its octal integer.
    mode = int(args.mode, 8)
    with os.fdopen(os.open(args.output, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, mode), 'w') as f:
        f.write(rendered_content)

    print(f"Successfully rendered {args.output} with mode {args.mode}")

if __name__ == "__main__":
    render_template()
