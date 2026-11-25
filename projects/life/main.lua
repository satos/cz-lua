-- CZ Lua sample: Conway's Game of Life (first bundled sample, 2026-11-25)
-- Grid size adapts to the actual screen via czlua.system.getSize().
-- One generation every 3 frames. keypressed("space") reseeds.

local CELL = 4
local W, H, grid, nxt, tick

local function seed()
  math.randomseed(20261125)
  grid, nxt = {}, {}
  for y = 1, H do
    grid[y], nxt[y] = {}, {}
    for x = 1, W do
      grid[y][x] = math.random() < 0.22
      nxt[y][x] = false
    end
  end
end

function load()
  local w, h = czlua.system.getSize()
  W, H = math.floor(w / CELL), math.floor(h / CELL)
  seed()
end

local function step()
  for y = 1, H do
    for x = 1, W do
      local n = 0
      for dy = -1, 1 do
        for dx = -1, 1 do
          if not (dx == 0 and dy == 0) then
            if grid[(y + dy - 1) % H + 1][(x + dx - 1) % W + 1] then n = n + 1 end
          end
        end
      end
      nxt[y][x] = grid[y][x] and (n == 2 or n == 3) or (n == 3)
    end
  end
  grid, nxt = nxt, grid
end

function update(dt)
  tick = (tick or 0) + 1
  if tick % 3 == 0 then step() end
end

function draw()
  czlua.graphics.clear(0.02, 0.02, 0.05)
  czlua.graphics.setColor(0.55, 0.95, 0.65)
  for y = 1, H do
    for x = 1, W do
      if grid[y][x] then
        czlua.graphics.rectangle("fill", (x - 1) * CELL, (y - 1) * CELL, CELL - 1, CELL - 1)
      end
    end
  end
end

function keypressed(key)
  if key == "space" then seed() end
end
