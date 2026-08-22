return function()
	filter {}

	-- Steam SDK import library (vendored in this component)
	filter 'architecture:x86'
		libdirs { "components/legitimacy/lib/" }
		links { "steam_api" }

	filter 'architecture:x64'
		libdirs { "components/legitimacy/lib/" }
		links { "steam_api64" }

	filter {}
end
