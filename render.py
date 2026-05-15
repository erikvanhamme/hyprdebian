import argparse
import os
from jinja2 import Template

def render_template():
    parser = argparse.ArgumentParser(description="Generic Jinja2 Renderer")
    parser.add_argument("template", help="Path to the Jinja2 template file")
    parser.add_argument("output", help="Path to the output file")
    parser.add_argument("-v", "--var", action="append", help="Variables in key=value format")

    args = parser.parse_args()

    # Convert key=value strings into a dictionary
    context = {}
    if args.var:
        for item in args.var:
            key, value = item.split("=", 1)
            print(f"DEBUG: Input Received -> Key: '{key}', Raw Value: '{value}'")
            # Convert "true"/"false" strings to actual Booleans for Jinja logic
            if value.lower() == "true":
                value = True
            elif value.lower() == "false":
                value = False
            print(f"DEBUG: Context Mapping -> {key}: {value} (Type: {type(value).__name__})")
            context[key] = value

    # Ensure output directory exists
    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)

    # Read, Render, and Write
    with open(args.template, "r") as f:
        template = Template(f.read())

    rendered_content = template.render(context)

    with open(args.output, "w") as f:
        f.write(rendered_content)
        os.chmod(args.output, 0o600) # Set permissions as per your bash 0600

    print(f"Successfully rendered {args.output}")

if __name__ == "__main__":
    render_template()
