return function()
	filter {}
	defines { 'STATE_FIVE' }

	dofile('components/citizen-server-state/init.lua')

	files_project(_ROOTPATH .. '/components/citizen-server-state-fivesv/') {
		'src/Component.cpp',
	}
end
