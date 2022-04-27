# shellcheck shell=bash disable=2120

function maximize_window {
	local args
	declare -A args=(
		[summary]="Maximize the given window"
		[0:window_id]="$__WF_MSG_WINDOW_ID_DESCRIPTION"
	)
	__BAPt_parse_arguments args "$@"
	wf-call maximize_view "${args[window_id]}" 1 | _o_none
}

function unmaximize_window {
	local args
	declare -A args=(
		[summary]="Reize window to size before previous maximization"
		[0:window_id]="$__WF_MSG_WINDOW_ID_DESCRIPTION"
	)
	__BAPt_parse_arguments args "$@"
	wf-call maximize_view "${args[window_id]}" 0 | _o_none
}

function minimize_window {
	local args
	declare -A args=(
		[summary]="Minimize the given window"
		[0:window_id]="$__WF_MSG_WINDOW_ID_DESCRIPTION"
	)
	__BAPt_parse_arguments args "$@"
	wf-call minimize_view "${args[window_id]}" 1 | _o_none
}

function unminimize_window {
	local args
	declare -A args=(
		[summary]="Resize window to size before previous minimization"
		[0:window_id]="$__WF_MSG_WINDOW_ID_DESCRIPTION"
	)
	__BAPt_parse_arguments args "$@"
	wf-call minimize_view "${args[window_id]}" 0 | _o_none
}

function focus_window {
	local args
	declare -A args=(
		[summary]="Focus the given window"
		[0:window_id]="$__WF_MSG_WINDOW_ID_DESCRIPTION"
	)
	__BAPt_parse_arguments args "$@"
	wf-call focus_view "${args[window_id]}" 1 | _o_none
}

function unfocus_window {
	local args
	declare -A args=(
		[summary]="Return window to focus state before previous focus"
		[0:window_id]="$__WF_MSG_WINDOW_ID_DESCRIPTION"
	)
	__BAPt_parse_arguments args "$@"
	wf-call focus_view "${args[window_id]}" 0 | _o_none
}

function move_window_to_workspace {
	local args
	declare -A args=(
		[summary]="Move the given window to the given workspace"
		[0:window_id]="$__WF_MSG_WINDOW_ID_DESCRIPTION"
		[1:x]="x-cord of destination workspace"
		[2:y]="y-cord of destination workspace"
	)
	__BAPt_parse_arguments args "$@"
	wf-call change_workspace_view "${args[window_id]}" "${args[x]}" "${args[y]}" | _o_none
}
