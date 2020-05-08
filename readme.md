# XML Parser
This is a simple and fast bare-bone XML parser with around 1000 lines of code with minimal amount of dependencies (std::string, std::list, std::stack and std::functional - can be skipped).

It's intended use is for well known and well formed data in constrained environments, like embedded environments and similar.

Note:
- no UTF-8 support
- no Schemas or validation

It can be used in either streaming or 'DOM' mode.
Within the parser there are actually two different implementation, see bottom part of .h file for what you can remove.

