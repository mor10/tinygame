-- Only what the stock parts cannot do: the gem is worth a lot and sparkles.
on.touch("player", "gem", function(player, gem)
  fx.sparkle(gem.x, gem.y)
  gem:remove()
  score.add(50)
end)
