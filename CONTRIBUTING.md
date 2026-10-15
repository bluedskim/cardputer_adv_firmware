# Contributing

Thanks for your interest in **cardputer_adv_firmware**.

## Before you start

- Read the [README](README.md) for the Fedora-oriented dev setup, PlatformIO targets, and the desktop simulator.
- Firmware and companion app changes should stay focused; avoid unrelated refactors in the same pull request.

## Build

From the repository root:

```bash
pio run -e m5stack-cardputer          # device firmware
pio run -e sim -t upload               # desktop simulator (SDL)
```

Android companion: see [companion/README.md](companion/README.md).

## Pull requests

1. Fork the repository and create a branch from `main`.
2. Describe what changed and how you tested it (device, sim, or companion as applicable).
3. Open a pull request against `main`.

## Issues

Use [GitHub Issues](https://github.com/bluedskim/cardputer_adv_firmware/issues) for bugs and feature requests. Include firmware or companion version, hardware (Cardputer-Adv), and steps to reproduce when reporting bugs.

## License

By contributing, you agree that your contributions are licensed under the [Apache License 2.0](LICENSE), the same license as the project.
