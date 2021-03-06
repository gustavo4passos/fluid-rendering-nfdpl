#pragma once

namespace fluidity
{

class Renderer 
{
public:
	Renderer() { }
	virtual auto Render() -> void = 0;
  virtual auto Update() -> void { };
};

}
