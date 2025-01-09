#!/usr/bin/pwsh

param(
	[Parameter(Position=0, Mandatory=$true)]
	[string]$Prefix,

	[int]$Days=30
)

openssl genpkey -algorithm ed25519 -out "$Prefix-private-key.pem"
openssl req -new -x509 -sha256 -key "$Prefix-private-key.pem" -subj "/CN=vasama.github.io" -days $Days -out "$Prefix-certificate.pem"
