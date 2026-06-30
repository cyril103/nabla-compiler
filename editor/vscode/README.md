# Nabla VS Code Extension

This extension provides local editor support for Nabla files:

- language detection for `*.nabla`;
- TextMate syntax highlighting;
- line comments, bracket matching, folding markers, and indentation rules;
- snippets for common language forms and stdlib entry points.

## Local Installation

Run it directly from the repository root with VS Code:

```bash
code --extensionDevelopmentPath=editor/vscode
```

Alternatively, open `editor/vscode` in VS Code and press `F5` to launch an
Extension Development Host.

To install it persistently, package this directory as a VSIX and install the
generated archive:

```bash
cd editor/vscode
npx @vscode/vsce package
code --install-extension nabla-vscode-0.1.0.vsix
```

## Scope

This is a static editor integration. It does not run `nablac`, provide
diagnostics, or implement a language server yet.
