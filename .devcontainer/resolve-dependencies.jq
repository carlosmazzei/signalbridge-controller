# A pin is either a shared version or a map of Debian architecture to version.
to_entries[] |
.key as $package |
(.value | if type == "object" then .[$architecture] else . end) as $version |
if ($version | type) == "string" and ($version | length) > 0 then
  "\($package)=\($version)"
else
  error("Missing pinned version for \($package) on \($architecture)")
end
