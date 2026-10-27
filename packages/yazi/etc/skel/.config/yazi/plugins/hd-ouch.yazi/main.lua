--- @since 26.9.1
-- The previewer of archives of HyprDebian: lists what is in an archive with ouch, in place of the
-- archive previewer of yazi, which needs 7-zip. It shows the tree of ouch list --tree, which puts
-- every item under its folder whatever the order in the archive, with the icon of each item. A file
-- that ouch compressed alone, such as notes.txt.gz, is no archive to ouch list; it shows the name
-- it decompresses to.

local M = {}

-- Shows a message in the preview, wrapped.
function M.msg(job, lines)
	local text = {}
	for _, line in ipairs(lines) do
		text[#text + 1] = ui.Line(line)
	end
	ya.preview_widget(job, ui.Text(text):area(job.area):wrap(ui.Wrap.YES))
end

-- An item of the tree: its name, its depth and whether it is a folder; nil for a line that is
-- none, such as the first, Archive: "<path>". Each level of the tree is three columns, "│  " or
-- "   ", before "├── " or "└── " and the name, which ends in / for a folder.
function M.item(line)
	local prefix, name = line:match("^(.-)├── (.*)$")
	if not prefix then
		prefix, name = line:match("^(.-)└── (.*)$")
	end
	if not prefix or name == "" then
		return nil
	end
	local columns = select(2, prefix:gsub("│", "")) + select(2, prefix:gsub(" ", ""))
	local is_dir = name:sub(-1) == "/"
	if is_dir then
		name = name:sub(1, -2)
	end
	return { name = name, depth = columns // 3, is_dir = is_dir }
end

-- The items in the archive from job.skip on, at most limit of them, and the number of items
-- before them. Without items, the lines ouch wrote on stderr, or nil when it could not start.
function M.list(job, limit)
	local child, err = Command("ouch")
		:arg({ "list", "--tree", "--no", tostring(job.file.path) })
		:stdin(Command.NULL)
		:stdout(Command.PIPED)
		:stderr(Command.PIPED)
		:spawn()
	if not child then
		return nil, 0, { "Failed to start ouch: " .. tostring(err) }
	end

	local items, skipped, stderr = {}, 0, {}
	repeat
		local line, event = child:read_line()
		if event == 0 then
			local item = M.item((line:gsub("\r?\n$", "")))
			if item and skipped < job.skip then
				skipped = skipped + 1
			elseif item then
				items[#items + 1] = item
			end
		elseif event == 1 then
			stderr[#stderr + 1] = line:gsub("\r?\n$", "")
		else
			break
		end
	until #items >= limit
	child:start_kill()
	return items, skipped, stderr
end

function M:peek(job)
	local limit = job.area.h
	local items, skipped, stderr = M.list(job, limit)
	if not items then
		return M.msg(job, stderr)
	elseif job.skip > 0 and #items < limit then
		return ya.emit("peek", { math.max(0, skipped + #items - limit), only_if = job.file.url, upper_bound = true })
	elseif #items == 0 and #stderr > 0 then
		if not table.concat(stderr, "\n"):find("Only archives", 1, true) then
			return M.msg(job, stderr)
		end
		items = { { name = job.file.url.stem, depth = 0, is_dir = false } }
	end

	local lines = {}
	for _, item in ipairs(items) do
		local name, depth = item.name, item.depth
		local icon = th.icon:match(File {
			url = Url(name),
			cha = Cha { mode = tonumber(item.is_dir and "40700" or "100644", 8) },
		})
		local span = " "
		if icon then
			span = ui.Span(" " .. icon.text .. " "):style(icon.style)
		end

		lines[#lines + 1] = ui.Line {
			string.rep(" │", depth),
			span,
			ui.truncate(name, { rtl = true, max = math.max(0, job.area.w - depth * 2 - ui.width(span)) }),
		}
	end

	ya.preview_widget(job, ui.Text(lines):area(job.area))
end

function M:seek(job) require("code"):seek(job) end

return M
